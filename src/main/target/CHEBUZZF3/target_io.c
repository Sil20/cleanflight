#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "drivers/timer.h"
#include "drivers/timer_impl.h"

const timerHardware_t timerHardware[USABLE_TIMER_CHANNEL_COUNT] = {
    // INPUTS CH1-8
    { TIM1,  GPIOA, Pin_8,  TIM_Channel_1, TIM1_CC_IRQn,            1, Mode_AF_PP_PD, GPIO_PinSource8,  GPIO_AF_6}, // PWM1  - PA8
    { TIM16, GPIOB, Pin_8,  TIM_Channel_1, TIM1_UP_TIM16_IRQn,      0, Mode_AF_PP_PD, GPIO_PinSource8,  GPIO_AF_1}, // PWM2  - PB8
    { TIM17, GPIOB, Pin_9,  TIM_Channel_1, TIM1_TRG_COM_TIM17_IRQn, 0, Mode_AF_PP_PD, GPIO_PinSource9,  GPIO_AF_1}, // PWM3  - PB9
    { TIM8,  GPIOC, Pin_6,  TIM_Channel_1, TIM8_CC_IRQn,            1, Mode_AF_PP_PD, GPIO_PinSource6,  GPIO_AF_4}, // PWM4  - PC6
    { TIM8,  GPIOC, Pin_7,  TIM_Channel_2, TIM8_CC_IRQn,            1, Mode_AF_PP_PD, GPIO_PinSource7,  GPIO_AF_4}, // PWM5  - PC7
    { TIM8,  GPIOC, Pin_8,  TIM_Channel_3, TIM8_CC_IRQn,            1, Mode_AF_PP_PD, GPIO_PinSource8,  GPIO_AF_4}, // PWM6  - PC8
    { TIM15, GPIOF, Pin_9,  TIM_Channel_1, TIM1_BRK_TIM15_IRQn,     0, Mode_AF_PP_PD, GPIO_PinSource9,  GPIO_AF_3}, // PWM7  - PF9
    { TIM15, GPIOF, Pin_10, TIM_Channel_2, TIM1_BRK_TIM15_IRQn,     0, Mode_AF_PP_PD, GPIO_PinSource10, GPIO_AF_3}, // PWM8  - PF10
    // OUTPUTS CH1-10
    { TIM4,  GPIOD, Pin_12, TIM_Channel_1, TIM4_IRQn,               0, Mode_AF_PP, GPIO_PinSource12,    GPIO_AF_2}, // PWM9  - PD12
    { TIM4,  GPIOD, Pin_13, TIM_Channel_2, TIM4_IRQn,               0, Mode_AF_PP, GPIO_PinSource13,    GPIO_AF_2}, // PWM10 - PD13
    { TIM4,  GPIOD, Pin_14, TIM_Channel_3, TIM4_IRQn,               0, Mode_AF_PP, GPIO_PinSource14,    GPIO_AF_2}, // PWM11 - PD14
    { TIM4,  GPIOD, Pin_15, TIM_Channel_4, TIM4_IRQn,               0, Mode_AF_PP, GPIO_PinSource15,    GPIO_AF_2}, // PWM12 - PD15
    { TIM2,  GPIOA, Pin_1,  TIM_Channel_2, TIM2_IRQn,               0, Mode_AF_PP, GPIO_PinSource1,     GPIO_AF_1}, // PWM13 - PA1
    { TIM2,  GPIOA, Pin_2,  TIM_Channel_3, TIM2_IRQn,               0, Mode_AF_PP, GPIO_PinSource2,     GPIO_AF_1}, // PWM14 - PA2
    { TIM2,  GPIOA, Pin_3,  TIM_Channel_4, TIM2_IRQn,               0, Mode_AF_PP, GPIO_PinSource3,     GPIO_AF_1}, // PWM15 - PA3
    { TIM3,  GPIOB, Pin_0,  TIM_Channel_3, TIM3_IRQn,               0, Mode_AF_PP, GPIO_PinSource0,     GPIO_AF_2}, // PWM16 - PB0
    { TIM3,  GPIOB, Pin_1,  TIM_Channel_4, TIM3_IRQn,               0, Mode_AF_PP, GPIO_PinSource1,     GPIO_AF_2}, // PWM17 - PB1
    { TIM3,  GPIOA, Pin_4,  TIM_Channel_2, TIM3_IRQn,               0, Mode_AF_PP, GPIO_PinSource4,     GPIO_AF_2}, // PWM18 - PA4

    { TIM1,  GPIOA, Pin_9,  TIM_Channel_2, TIM1_CC_IRQn,            0, 0,          ~0,                  ~0},        // TIMER
};

#define TIMER_APB1_PERIPHERALS (RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4)
#define TIMER_APB2_PERIPHERALS (RCC_APB2Periph_TIM1 | RCC_APB2Periph_TIM8 | RCC_APB2Periph_TIM15 | RCC_APB2Periph_TIM16 | RCC_APB2Periph_TIM17)
#define TIMER_AHB_PERIPHERALS (RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC | RCC_AHBPeriph_GPIOD | RCC_AHBPeriph_GPIOF)

void timerInitTarget(void)
{
    RCC_APB1PeriphClockCmd(TIMER_APB1_PERIPHERALS, ENABLE);
    RCC_APB2PeriphClockCmd(TIMER_APB2_PERIPHERALS, ENABLE);
    RCC_AHBPeriphClockCmd(TIMER_AHB_PERIPHERALS, ENABLE);
}
