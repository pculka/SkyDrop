#include "bt.h"

#include "../../fc/conf.h"

#include "pan1322.h"
#include "pan1026.h"

bool bt_autodetect = false;
uint8_t bt_module_type = BT_PAN1322;

pan1026 bt_pan1026;
pan1322 bt_pan1322;

Usart bt_uart;

volatile bool bt_device_connected = false;

#define BT_MOD_STATE_OFF	0
#define BT_MOD_STATE_INIT	1
#define BT_MOD_STATE_OK		2

volatile uint8_t bt_module_state = BT_MOD_STATE_OFF;

ISR(BT_CTS_PIN_INT)
{
//	DEBUG("CTS\n");
	if (bt_module_type == BT_PAN1322)
		bt_pan1322.TxResume();

	if (bt_module_type == BT_PAN1026)
		bt_pan1026.TxResume();
}

void bt_pan1322_init()
{
	DEBUG("bt_pan1322_init\n");

	bt_pan1322.Init(&bt_uart);
}

void bt_pan1026_init()
{
	DEBUG("bt_pan1026_init\n");

	bt_pan1026.Init(&bt_uart);
}

void bt_module_deinit()
{
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	bt_irgh(BT_IRQ_DEINIT, 0);
	bt_uart.Stop();
	BT_UART_PWR_OFF;
}

void bt_module_reset()
{
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	_delay_ms(200);
	GpioWrite(BT_EN, HIGH);
	_delay_ms(200);
	GpioWrite(BT_RESET, HIGH);
	_delay_ms(300);
}

void bt_module_init()
{
	//enable bt uart
	BT_UART_PWR_ON;

	//module specific code
	switch (bt_module_type)
	{
		case(BT_PAN1322):
			bt_pan1322_init();
		break;

		case(BT_PAN1026):
			bt_pan1026_init();
		break;

		default: //UNKNOWN module
			bt_autodetect = true;
			bt_module_type = BT_PAN1322;
			bt_pan1322_init();
		break;
	}

}

void bt_init()
{
	DEBUG("bt_init\n");

	//get module type
	eeprom_busy_wait();
	bt_module_type = eeprom_read_byte(&config_ro.bt_module_type);

	//init bt_uart
	bt_uart.InitBuffers(BUFFER_SIZE * 2, BUFFER_SIZE);

	//pin init
	GpioSetDirection(BT_EN, OUTPUT);
	GpioSetDirection(BT_RESET, OUTPUT);
	GpioSetDirection(BT_RTS, OUTPUT);

	//power is off
	GpioWrite(BT_RTS, LOW);
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	//IRQ init
	GpioSetDirection(BT_CTS, INPUT);
	GpioSetPull(BT_CTS, gpio_pull_down);
	GpioSetInterrupt(BT_CTS, gpio_interrupt1, gpio_rising);
}

bool bt_device_active()
{
	return bt_device_connected;
}

void bt_unknown_parser()
{
	while (!bt_uart.isRxBufferEmpty())
	{
		uint8_t c = bt_uart.Read();
		DEBUG(">> %02X %c ??\n", c, c);
	}
}

void bt_step()
{
	if (bt_module_state == BT_MOD_STATE_OFF)
		return;

	if (bt_module_type == BT_PAN1322)
		bt_pan1322.Step();

	if (bt_module_type == BT_PAN1026)
		bt_pan1026.Step();

	if (bt_module_type == BT_UNKNOWN)
		bt_unknown_parser();
}

void bt_send(char * str)
{
	if (!bt_device_connected)
		return;

	if (bt_module_type == BT_PAN1322)
		bt_pan1322.SendString(str);

	if (bt_module_type == BT_PAN1026)
		bt_pan1026.SendString(str);
}

void bt_irgh(uint8_t type, uint8_t * buf)
{
	uint8_t old_type;

	switch(type)
	{
		case(BT_IRQ_INIT):
		DEBUG("BT_MOD_STATE_INIT\n");
			bt_module_state = BT_MOD_STATE_INIT;
		break;
		case(BT_IRQ_INIT_OK):
			DEBUG("BT_MOD_STATE_OK\n");
			bt_module_state = BT_MOD_STATE_OK;
			bt_autodetect = false;
		break;
		case(BT_IRQ_DEINIT):
			DEBUG("BT_MOD_STATE_OFF\n");
			bt_module_state = BT_MOD_STATE_OFF;
		break;
		case(BT_IRQ_CONNECTED):
			DEBUG("BT_IRQ_CONNECTED\n");
			gui_showmessage_P(PSTR("Bluetooth\nconnected"));
			bt_device_connected = true;
		break;
		case(BT_IRQ_DISCONNECTED):
			DEBUG("BT_IRQ_DISCONNECTED\n");
			gui_showmessage_P(PSTR("Bluetooth\ndisconnected"));
			bt_device_connected = false;
		break;
		case(BT_IRQ_RESET):
			DEBUG("BT_IRQ_RESET\n");
			bt_device_connected = false;
		break;
		case(BT_IRQ_INIT_FAIL):
			DEBUG("BT_IRQ_INIT_FAIL!\n");
			if (bt_autodetect)
			{
				old_type = bt_module_type;

				bt_module_deinit();
				DEBUG("de_init_now\n");
				bt_module_type = BT_UNKNOWN;
				_delay_ms(500);

				BT_UART_PWR_ON;

				if (old_type == BT_PAN1322)
				{
					bt_module_type = BT_PAN1026;
					bt_pan1026_init();
				}

				if (old_type == BT_PAN1026)
				{
					bt_module_type = BT_PAN1322;
					bt_pan1322_init();
				}

			}
			else
			{
//				gui_showmessage_P(PSTR("Bluetooth\nInit failed!"));
			}
		break;
	}
}

bool bt_selftest()
{
	return (bt_module_state == BT_MOD_STATE_OK);
}

uint8_t bt_get_module_type()
{
	return bt_module_type;
}
