
#include "nrf.h"
#include "app_error.h"
#include "compiler_abstraction.h"
#include "nordic_common.h"
#include "simble.h"
#include "onboard-led.h"





/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 *
 * Function is implemented as weak so that it can be overwritten by custom application error handler
 * when needed.
 */

/*lint -save -e14 */
__WEAK void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
	segger_rtt_printf("err: %d, line: %d, file: %s\r\n", error_code, line_num, p_file_name);
	/*__disable_irq();
	for (;;) {
		nrf_delay_ms(200);
		onboard_led(ONBOARD_LED_TOGGLE);
	}*/
    // On assert, the system can only recover with a reset.



}
/*lint -restore */
