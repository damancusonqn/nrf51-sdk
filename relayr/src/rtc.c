#include "simble.h"


// Configure the Tick interval, 0x20 = 32.768/32 = 1.024
#define RTC_PRESCALER 31u

struct rtc_x {
  uint32_t period;  //24 bits, max value: 16777216 (~4 hours @ 1ms tick)
  uint8_t enabled;
};

struct rtc_ctx {
	uint8_t timers_count;  //Max 4
};

/* RTC : the default TICK_INTERVAL is 1ms, the module can manage up to 4 compare
      registers */
void
rtc_init(void)
{
  sd_nvic_ClearPendingIRQ(RTC1_IRQn);
  sd_nvic_SetPriority(RTC1_IRQn, NRF_APP_PRIORITY_LOW);
  sd_nvic_EnableIRQ(RTC1_IRQn);

  // Config. CC[0] module to generate interrupts
  NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  //The LFCLK runs at 32.768 Hz, divided by (PRESCALER+1) is 1.024 ms
  NRF_RTC1->PRESCALER = RTC_PRESCALER;
}

/* The RTC1 instance IRQ handler*/
void
RTC1_IRQHandler(void)
{
	if (NRF_RTC1->EVENTS_COMPARE[0] == 0)
		return;
	NRF_RTC1->EVENTS_COMPARE[0] = 0;

  //Toggle pin2 for test
  NRF_GPIO->OUT ^= (1 << 2);

}

  // __O  uint32_t  TASKS_START;                       /*!< Start RTC Counter.                                                    */
  // __O  uint32_t  TASKS_STOP;                        /*!< Stop RTC Counter.                                                     */
  // __O  uint32_t  TASKS_CLEAR;                       /*!< Clear RTC Counter.                                                    */
  // __O  uint32_t  TASKS_TRIGOVRFLW;                  /*!< Set COUNTER to 0xFFFFFFF0.                                            */
  // __I  uint32_t  RESERVED0[60];
  // __IO uint32_t  EVENTS_TICK;                       /*!< Event on COUNTER increment.                                           */
  // __IO uint32_t  EVENTS_OVRFLW;                     /*!< Event on COUNTER overflow.                                            */
  // __I  uint32_t  RESERVED1[14];
  // __IO uint32_t  EVENTS_COMPARE[4];                 /*!< Compare event on CC[n] match.                                         */
  // __I  uint32_t  RESERVED2[109];
  // __IO uint32_t  INTENSET;                          /*!< Interrupt enable set register.                                        */
  // __IO uint32_t  INTENCLR;                          /*!< Interrupt enable clear register.                                      */
  // __I  uint32_t  RESERVED3[13];
  // __IO uint32_t  EVTEN;                             /*!< Configures event enable routing to PPI for each RTC event.            */
  // __IO uint32_t  EVTENSET;                          /*!< Enable events routing to PPI. The reading of this register gives
                                                        //  the value of EVTEN.                                                   */
  // __IO uint32_t  EVTENCLR;                          /*!< Disable events routing to PPI. The reading of this register
                                                        //  gives the value of EVTEN.                                             */
  // __I  uint32_t  RESERVED4[110];
  // __I  uint32_t  COUNTER;                           /*!< Current COUNTER value.                                                */
  // __IO uint32_t  PRESCALER;                         /*!< 12-bit prescaler for COUNTER frequency (32768/(PRESCALER+1)).
                                                        //  Must be written when RTC is STOPed.                                   */
  // __I  uint32_t  RESERVED5[13];
  // __IO uint32_t  CC[4];                             /*!< Capture/compare registers.                                            */
  // __I  uint32_t  RESERVED6[683];
  // __IO uint32_t  POWER;                             /*!< Peripheral power control.                                             */
// } NRF_RTC_Type;
