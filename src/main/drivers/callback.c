#include <string.h>
#include <stdbool.h>

#include "platform.h"
#include "build_config.h"
#include "common/utils.h"

#include "gpio.h"

#include "callback.h"

#define CALLBACK_MAX 32

callbackRec_t* callbackEntries[CALLBACK_MAX];
int callbackCount;

#if CALLBACK_MAX>32
# error "Currently only 32 callbacks are supported"
#endif

uint32_t callbackTriggers[(CALLBACK_MAX+(CALLBACK_MAX-1))/32];
uint32_t callbackFree[(CALLBACK_MAX+(CALLBACK_MAX-1))/32];

void callbackEmptyFn(callbackRec_t *self)
{
    UNUSED(self);
    // this is empy function to set unused handlers
}

callbackRec_t callbackEmptyRec = {callbackEmptyFn, -1};

void callbackInit(void)
{
// TODO debug
    gpio_config_t cfg;
    
    cfg.pin = Pin_6;
    cfg.mode = Mode_Out_PP;
    cfg.speed = Speed_2MHz;
    gpioInit(GPIOB, &cfg);
    

    memset(callbackTriggers, 0, sizeof(callbackTriggers));
    memset(callbackFree, 0xff, sizeof(callbackTriggers));
    for(int i=0;i<CALLBACK_MAX;i++)
        callbackEntries[i]=&callbackEmptyRec;
    callbackCount=0;
    NVIC_SetPriority(PendSV_IRQn, 0x0f);  // lowest priority
}

void callbackRegister(callbackRec_t *self, callbackFun_t *fn)
{
    int id=__builtin_ffs(callbackFree[0])-1;
    if(id<0)      // no free callback available
        return;
    self->fn=fn;
    self->id=id;
    callbackEntries[id]=self;
    callbackFree[0] &= ~(1 << id);
}

void callbackRelease(callbackRec_t *self)
{
    callbackTriggers[0] &= ~(1 << self->id);         // clear trigger if set
    callbackEntries[self->id]=&callbackEmptyRec;     // remove entry
    callbackFree[0] |= 1 << self->id;                // mark his position as free
}

void callbackTrigger(callbackRec_t *self)
{
    callbackTriggers[0] |= 1 << self->id;
    SCB->ICSR=SCB_ICSR_PENDSVSET;
}

static void callbackCall(void) {
    while(callbackTriggers[0]) {
        uint8_t idx=31-__builtin_clz(callbackTriggers[0]);
        callbackTriggers[0]&=~(1<<idx);
        callbackEntries[idx]->fn(callbackEntries[idx]);
    }
}

void PendSV_Handler(void)
{
    digitalHi(GPIOB, Pin_6);
    callbackCall();
    digitalLo(GPIOB, Pin_6);  
}