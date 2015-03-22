#include "simble.h"
#include "rtc.h"


// Configure the Tick interval, 0x20 = 32.768/32 = 1.024
#define RTC_PRESCALER 31u


static struct rtc_ctx *ctx;

/* RTC : the default TICK_INTERVAL is 1ms, the module can manage up to 4 compare
      registers */
void
rtc_init(struct rtc_ctx *c)
{
  ctx = c;

  sd_nvic_ClearPendingIRQ(RTC1_IRQn);
  sd_nvic_SetPriority(RTC1_IRQn, NRF_APP_PRIORITY_LOW);
  sd_nvic_EnableIRQ(RTC1_IRQn);

  //The LFCLK runs at 32.768 Hz, divided by (PRESCALER+1) = 1.024 ms
  NRF_RTC1->PRESCALER = RTC_PRESCALER;

  NRF_RTC1->CC[0] = ctx->rtc_x[0].period;

  // Reset the Counter
  NRF_RTC1->TASKS_CLEAR = 1;
  // Start the RTC1
  NRF_RTC1->TASKS_START = 1;

  NRF_RTC1->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
  // Config. CC[0] module to generate interrupts
  NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  // Disable the Event routing to the PPI to save power
  NRF_RTC1->EVTEN = 0;
}

void rtc_oneshot_timer(){
//TODO: implement this
}

/* The RTC1 instance IRQ handler*/
void
RTC1_IRQHandler(void)
{
  NRF_GPIO->OUT ^= (1 << 1);

  for (uint8_t i = 0; i < ctx->used_timers; i++){

    if (NRF_RTC1->EVENTS_COMPARE[i] != 0)
    {
        // prepare the comparator for the next interval
        NRF_RTC1->CC[i] += ctx->rtc_x[i].period;

        // clear the event CC_x
        NRF_RTC1->EVENTS_COMPARE[i] = 0;
        //NRF_RTC1->INTENCLR = RTC_INTENCLR_COMPARE0_Msk;

        //Toggle pin2 for test
        NRF_GPIO->OUT ^= (1 << 2);

        ctx->rtc_x[i].cb(ctx); //TODO:check this
        //sd_nvic_ClearPendingIRQ(RTC1_IRQn);
    }
  }
}
