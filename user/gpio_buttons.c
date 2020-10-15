#include "gpio_buttons.h"
#include "user_interface.h"
#include "c_types.h"
#include <gpio.h>
#include <ets_sys.h>
#include <esp82xxutil.h>

void gpio_pin_intr_state_set(uint32 i, GPIO_INT_TYPE intr_state);

volatile uint8_t LastGPIOState;

//static const uint8_t GPID[] = { 0, 12 };
static const uint8_t GPID[] = {0, 12, 5};
static const uint8_t Func[] = {FUNC_GPIO0, FUNC_GPIO12, FUNC_GPIO5 };
static const int  Periphs[] = {PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_GPIO5_U };





void ICACHE_FLASH_ATTR SetupGPIO()
{
	printf( "SETTING UP GPIO\n" );

	int i;
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts

	for( i = 0; i < 3; i++ )
	{
		PIN_FUNC_SELECT(Periphs[i], Func[i]);
		PIN_DIR_INPUT = 1<<GPID[i];
		printf("set GPIO %d as input\n", GPID[i]);
	}
	LastGPIOState = GetButtons();
 	printf( "Setup GPIO Complete\n" );
}


uint8_t GetButtons()
{
	uint8_t ret = 0;
	int i;
	uint32_t pin = PIN_IN;
	int mask = 1;
	for( i = 0; i < 2; i++ )
	{
		ret |= (pin & (1<<GPID[i]))?mask:0;
		printf("[ret %d, i %d, mask %d]", ret, i , mask)
		mask <<= 1;
	}
	ret ^= ~32; //GPIO15's logic is inverted.  Don't flip it but flip everything else.
	printf("got button %d\n", ret);
	return ret;
}
