/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Authors:
 * Dominic Clifton - Serial port abstraction, Separation of common STM32 code for cleanflight, various cleanups.
 * Hamasaki/Timecop - Initial baseflight code
*/
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "platform.h"

#include "build_config.h"

#include "common/utils.h"
#include "gpio.h"
#include "inverter.h"

#include "serial.h"
#include "serial_uart.h"
#include "serial_uart_impl.h"

static void uartReconfigure(uartPort_t *self)
{
    USART_InitTypeDef USART_InitStructure;
    USART_Cmd(self->USARTx, DISABLE);

    USART_InitStructure.USART_BaudRate = self->port.baudRate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    if (self->port.mode & MODE_SBUS) {
        USART_InitStructure.USART_StopBits = USART_StopBits_2;
        USART_InitStructure.USART_Parity = USART_Parity_Even;
    } else {
        USART_InitStructure.USART_StopBits = USART_StopBits_1;
        USART_InitStructure.USART_Parity = USART_Parity_No;
    }
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = 0;
    if (self->port.state & STATE_RX)
        USART_InitStructure.USART_Mode |= USART_Mode_Rx;
    if (self->port.state & STATE_TX)
        USART_InitStructure.USART_Mode |= USART_Mode_Tx;

    USART_Init(self->USARTx, &USART_InitStructure);
    USART_Cmd(self->USARTx, ENABLE);
}

serialPort_t *uartOpen(USART_TypeDef *USARTx, const serialPortConfig_t *config)
{
    uartPort_t *self;

#ifdef INVERTER
    if (config->mode & MODE_INVERTED && USARTx == INVERTER_USART) {
        // Enable hardware inverter if available.
        INVERTER_ON;
    }
#endif

    if(0) {
#ifdef USE_USART1
    } else if (USARTx == USART1) {
        self = serialUSART1(config);
#endif
#ifdef USE_USART2
    } else if (USARTx == USART2) {
        self = serialUSART2(config);
#endif
#ifdef USE_USART3
    } else if (USARTx == USART3) {
        self = serialUSART3(config);
#endif
    } else {
        return NULL;
    }

    self->txDMAEmpty = true;

    // common serial initialisation code should move to serialPort::init()
    self->port.rxBufferHead = self->port.rxBufferTail = 0;
    self->port.txBufferHead = self->port.txBufferTail = 0;
    // callback works for IRQ-based RX ONLY
    self->port.rxCallback = config->rxCallback;
    self->port.baudRate = config->baudRate;          // TODO - recalculate actual baudrate

    // setup initial port state
    self->port.state = 0;
    if(config->mode & MODE_RX) {
        self->port.state |= STATE_RX;
        self->port.mode |= MODE_RX;
    }
    if(config->mode & MODE_TX) {
        self->port.state |= STATE_TX;
        self->port.mode |= MODE_TX;
    }

// FIXME use inversion on STM32F3
// TODO - use singlewire mode (supported both by 10x and 30x)
    uartReconfigure(self);

    // Receive DMA or IRQ
    DMA_InitTypeDef DMA_InitStructure;

    if (self->port.mode & MODE_RX) {
        if (self->rxDMAChannel && config->mode & MODE_U_DMARX) {
            DMA_StructInit(&DMA_InitStructure);
            DMA_InitStructure.DMA_PeripheralBaseAddr = self->rxDMAPeripheralBaseAddr;
            DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
            DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
            DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
            DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
            DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
            DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;

            DMA_InitStructure.DMA_BufferSize = self->port.rxBufferSize;
            DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
            DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
            DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)self->port.rxBuffer;
            DMA_DeInit(self->rxDMAChannel);
            DMA_Init(self->rxDMAChannel, &DMA_InitStructure);
            DMA_Cmd(self->rxDMAChannel, ENABLE);
            USART_DMACmd(self->USARTx, USART_DMAReq_Rx, ENABLE);
            self->rxDMAPos = DMA_GetCurrDataCounter(self->rxDMAChannel);

            self->port.mode |= MODE_U_DMARX;
        } else {
            USART_ClearITPendingBit(self->USARTx, USART_IT_RXNE);
            USART_ITConfig(self->USARTx, USART_IT_RXNE, ENABLE);
        }
    }

    // Transmit DMA or IRQ
    if (self->port.mode & MODE_TX) {
        if (self->txDMAChannel && config->mode & MODE_U_DMATX) {
            DMA_StructInit(&DMA_InitStructure);
            DMA_InitStructure.DMA_PeripheralBaseAddr = self->txDMAPeripheralBaseAddr;
            DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
            DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
            DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
            DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
            DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
            DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;

            DMA_InitStructure.DMA_BufferSize = self->port.txBufferSize;
            DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
            DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
            DMA_DeInit(self->txDMAChannel);
            DMA_Init(self->txDMAChannel, &DMA_InitStructure);
            DMA_ITConfig(self->txDMAChannel, DMA_IT_TC, ENABLE);
            DMA_SetCurrDataCounter(self->txDMAChannel, 0);
            self->txDMAChannel->CNDTR = 0;
            USART_DMACmd(self->USARTx, USART_DMAReq_Tx, ENABLE);

            self->port.mode |= MODE_U_DMATX;
        } else {
            USART_ITConfig(self->USARTx, USART_IT_TXE, ENABLE);
        }
    }

    USART_Cmd(self->USARTx, ENABLE);

    if (mode & MODE_SINGLEWIRE)
        USART_HalfDuplexCmd(s->USARTx, ENABLE);
    else
        USART_HalfDuplexCmd(s->USARTx, DISABLE);

    return &self->port;
}

// this function will need critical section if extended serial functions are implemented
void uartUpdateState(serialPort_t *serial, portState_t andMask, portState_t orMask)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    portState_t newState = (self->port.state & andMask) | orMask;
    self->port.state = newState;
    uartReconfigure(self);
}

void uartConfigure(serialPort_t *serial, const serialPortConfig_t *config)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    // just call reconfigure now. keep this in sync with uartRelease
    // TODO - we should reaquire DMA channels
    if(config->mode == 0)  // check for dummy config
        return;

    self->port.mode = config->mode;
    self->port.baudRate = config->baudRate;
    self->port.state = 0;
    if(self->port.mode & MODE_RX)
        self->port.state |= STATE_RX;
    if(self->port.mode & MODE_TX)
        self->port.state |= STATE_TX;
    uartReconfigure(self);
}

void uartRelease(serialPort_t *serial)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    uartUpdateState(&self->port, 0, 0);
    // DMA channels should be released
    USART_Cmd(self->USARTx, DISABLE);
    uartReconfigure(self);
    self->port.mode = 0;
}

void uartGetConfig(serialPort_t *serial, serialPortConfig_t* config)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);

    config->baudRate = self->port.baudRate;  // TODO - use actual baudrate
    config->mode = self->port.mode;
    config->rxCallback = self->port.rxCallback;
}

void uartStartTxDMA(uartPort_t *self)
{
    self->txDMAChannel->CMAR = (uint32_t)&self->port.txBuffer[self->port.txBufferTail];
    // TODO - data passed to DMA transfer are 'released' from queue immediately and could be overwritten. txBufferTail should be moved only after transfer is complete
    //  but beware that whole queue may be pending then. half interrupt can help
    // usart_write does not check buffer space anyway now ...
    if (self->port.txBufferHead > self->port.txBufferTail) {
        self->txDMAChannel->CNDTR = self->port.txBufferHead - self->port.txBufferTail;
        self->port.txBufferTail = self->port.txBufferHead;
    } else {
        self->txDMAChannel->CNDTR = self->port.txBufferSize - self->port.txBufferTail;
        self->port.txBufferTail = 0;
    }
    self->txDMAEmpty = false;
    DMA_Cmd(self->txDMAChannel, ENABLE);
}

void uartIrqHandler(uartPort_t *self)
{
#if defined(STM32F10X)
    uint16_t flags = self->USARTx->SR;
#elif defined(STM32F303)
    uint32_t flags = self->USARTx->ISR;
#else
# error "Unknown CPU"
#endif

    if (!(self->port.mode & MODE_U_DMARX) && (flags & USART_IT_RXNE)) {
        if (self->port.rxCallback) {
            self->port.rxCallback(USART_ReceiveData(self->USARTx));
        } else {
            self->port.rxBuffer[self->port.rxBufferHead] = USART_ReceiveData(self->USARTx);
            self->port.rxBufferHead = (self->port.rxBufferHead + 1) & (self->port.rxBufferSize - 1);
        }
    }

    if (!(self->port.mode & MODE_U_DMATX) && (flags & USART_IT_TXE)) {
        if (self->port.txBufferTail != self->port.txBufferHead) {
            USART_SendData(self->USARTx, self->port.txBuffer[self->port.txBufferTail]);
            self->port.txBufferTail = (self->port.txBufferTail + 1) & (self->port.txBufferSize - 1);
        } else {
            USART_ITConfig(self->USARTx, USART_IT_TXE, DISABLE);
        }
    }
#ifdef STM32F303
    // TODO - is this really neccesary?
    if (flags & USART_FLAG_ORE)
    {
        USART_ClearITPendingBit (self->USARTx, USART_IT_ORE);
    }
#endif
}

// interface implemenatition

bool isUartTransmitBufferEmpty(serialPort_t *serial)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    if (self->port.mode & MODE_U_DMATX)
        return self->txDMAEmpty;
    else
        return self->port.txBufferTail == self->port.txBufferHead;
}

void uartPutc(serialPort_t *serial, uint8_t ch)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    // TODO - check for full buffer

    self->port.txBuffer[self->port.txBufferHead] = ch;
    self->port.txBufferHead = (self->port.txBufferHead + 1) & (self->port.txBufferSize - 1);

    if (self->port.mode & MODE_U_DMATX) {
        if (!(self->txDMAChannel->CCR & 1))
            uartStartTxDMA(self);
    } else {
        USART_ITConfig(self->USARTx, USART_IT_TXE, ENABLE);
    }
}

int uartTotalBytesWaiting(serialPort_t *serial)
{
    uartPort_t *self = container_of(serial, uartPort_t, port);
    if (self->port.mode & MODE_U_DMARX) {
        return (self->rxDMAChannel->CNDTR - self->rxDMAPos) & (self->port.rxBufferSize - 1);
    } else {
        return (self->port.rxBufferHead - self->port.rxBufferTail) & (self->port.rxBufferSize - 1);
    }
}

int uartGetc(serialPort_t *serial)
{
    uint8_t ch;
    uartPort_t *self = container_of(serial, uartPort_t, port);

    // TODO - this function shoud check for empty buffer

    if (self->port.mode & MODE_U_DMARX) {
        ch = self->port.rxBuffer[self->port.rxBufferSize - self->rxDMAPos];
        if (--self->rxDMAPos == 0)
            self->rxDMAPos = self->port.rxBufferSize;
    } else {
        ch = self->port.rxBuffer[self->port.rxBufferTail];
        self->port.rxBufferTail = (self->port.rxBufferTail + 1) & (self->port.rxBufferSize - 1);
    }

    return ch;
}

const struct serialPortVTable uartVTable = {
    .isTransmitBufferEmpty = isUartTransmitBufferEmpty,
    .putc = uartPutc,
    .totalBytesWaiting = uartTotalBytesWaiting,
    .getc = uartGetc,

    .release = uartRelease,
    .configure = uartConfigure,
    .getConfig = uartGetConfig,
    .updateState = uartUpdateState
};
