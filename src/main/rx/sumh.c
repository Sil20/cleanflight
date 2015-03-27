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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "platform.h"

#include "build_config.h"

#include "drivers/system.h"

#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "io/serial.h"

#include "rx/rx.h"
#include "rx/sumh.h"

// driver for SUMH receiver using UART2

#define SUMH_MAX_CHANNEL_COUNT 8
#define SUMH_FRAME_SIZE 21

static bool sumhFrameDone = false;

static uint8_t sumhFrame[SUMH_FRAME_SIZE];
static uint32_t sumhChannels[SUMH_MAX_CHANNEL_COUNT];

static void sumhDataReceive(uint16_t c);
static uint16_t sumhReadRawRC(rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan);

static serialPort_t *sumhPort;
static const serialPortMode_t sumhConfig = {
    .mode = MODE_RX | (MODE_DEFAULT_FAST & ~MODE_U_DMARX),  // don't enable DMA, we need rxCallback
    .baudRate = 115200,
    .rxCallback = sumhDataReceive
};

static void sumhDataReceive(uint16_t c);
static uint16_t sumhReadRawRC(rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan);



bool sumhInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback)
{
    UNUSED(rxConfig);

    if (callback)
        *callback = sumhReadRawRC;

    rxRuntimeConfig->channelCount = SUMH_MAX_CHANNEL_COUNT;

    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RX_SERIAL);
    if (!portConfig) {
        return false;
    }

    sumhPort = openSerialPort(portConfig->identifier, FUNCTION_RX_SERIAL, &sumhConfig);

    return sumhPort != NULL;
}

// Receive ISR callback
static void sumhDataReceive(uint16_t c)
{
    uint32_t sumhTime;
    static uint32_t sumhTimeLast, sumhTimeInterval;
    static uint8_t sumhFramePosition;

    sumhTime = micros();
    sumhTimeInterval = sumhTime - sumhTimeLast;
    sumhTimeLast = sumhTime;
    if (sumhTimeInterval > 5000) {
        sumhFramePosition = 0;
    }

    sumhFrame[sumhFramePosition] = (uint8_t) c;
    if (sumhFramePosition == SUMH_FRAME_SIZE - 1) {
        // FIXME at this point the value of 'c' is unused and un tested, what should it be, is it important?
        sumhFrameDone = true;
    } else {
        sumhFramePosition++;
    }
}

uint8_t sumhFrameStatus(void)
{
    uint8_t channelIndex;

    if (!sumhFrameDone) {
        return SERIAL_RX_FRAME_PENDING;
    }

    sumhFrameDone = false;

    if (!((sumhFrame[0] == 0xA8) && (sumhFrame[SUMH_FRAME_SIZE - 2] == 0))) {
        return SERIAL_RX_FRAME_PENDING;
    }

    for (channelIndex = 0; channelIndex < SUMH_MAX_CHANNEL_COUNT; channelIndex++) {

        sumhChannels[channelIndex] = (((uint32_t)(sumhFrame[(channelIndex << 1) + 3]) << 8)
                + sumhFrame[(channelIndex << 1) + 4]) / 6.4 - 375;
    }
    return SERIAL_RX_FRAME_COMPLETE;
}

static uint16_t sumhReadRawRC(rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan)
{
    UNUSED(rxRuntimeConfig);

    if (chan >= SUMH_MAX_CHANNEL_COUNT) {
        return 0;
    }

    return sumhChannels[chan];
}
