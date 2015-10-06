#include "common.h"
#include "drivers/uart.h"
#include "drivers/storage/storage.h"
#include <string.h>
#include "gui/gui.h"
#include "fc/conf.h"


struct app_info ee_fw_info __attribute__ ((section(".fw_info")));
struct app_info fw_info;

void print_fw_info()
{
	eeprom_busy_wait();
	eeprom_read_block(&fw_info, &ee_fw_info, sizeof(fw_info));

	DEBUG("App name: ");
	for (uint8_t i = 0; i < APP_INFO_NAME_len; i++)
	{
		uint8_t c = fw_info.app_name[i];
		if (c < 32 || c > 126)
			c = '_';
		DEBUG("%c", c);
	}
	DEBUG("\n");
}

void test_memory()
{
	DEBUG("Free RAM now ... %d\n", freeRam());
}

//----------------------------------------------------------

void turnoff_subsystems()
{
	//PORTA
	PR.PRPA = 0b00000111;
	//PORTB
	PR.PRPB = 0b00000111;
	//PORTC
	PR.PRPC = 0b01111111;
	//PORTD
	PR.PRPD = 0b01111111;
	//PORTE
	PR.PRPE = 0b01111111;
	//PORTF
	PR.PRPF = 0b01111111;
	//PRGEN - RTC must stay on
	PR.PRGEN = 0b01011011;
}

//----------------------------------------------------------

DataBuffer::DataBuffer(uint16_t size)
{
	this->size = size;
	this->length = 0;
	this->write_index = 0;
	this->read_index = 0;

	this->data = new uint8_t[size];
	if (this->data == NULL)
	{
		this->size = 0;
	}
}

DataBuffer::~DataBuffer()
{
	test_memory();
	if (this->size)
	{
		DEBUG("Doing nothing!\n");
		delete[] this->data;
	}
	test_memory();
}

uint16_t DataBuffer::Read(uint16_t len, uint8_t * * data)
{
	(*data) = this->data + this->read_index;

	if (len > this->length)
		len = this->length;

	if (this->read_index + len > this->size)
		len = this->size - this->read_index;

	this->read_index = (this->read_index + len) % this->size;
	this->length -= len;

	return len;
}

bool DataBuffer::Write(uint16_t len, uint8_t * data)
{
//	for (uint16_t i=0;i<len;i++)
//		DEBUG(" %02X", data[i]);
//	DEBUG(" << \n");

	if (this->size - this->length < len)
		return false;

	if (this->write_index + len > this->size)
	{
		uint16_t first_len = this->size - this->write_index;
		memcpy(this->data + this->write_index, data, first_len);
		memcpy(this->data, data + first_len, len - first_len);
	}
	else
	{
		memcpy(this->data + this->write_index, data, len);
	}

	this->write_index = (this->write_index + len) % this->size;
	this->length += len;

//	DEBUG("wi: %d ", this->write_index);
//	DEBUG("ri: %d ", this->read_index);
//	DEBUG("ln: %d\n", this->length);
//
//	DEBUG("S");
//	for (uint16_t i=this->read_index;i < this->read_index + this->length;i++)
//		DEBUG(" %02X", this->data[i]);
//	DEBUG("\n");
//	DEBUG("\n");

	return true;
}

uint16_t DataBuffer::Length()
{
	return this->length;
}

void DataBuffer::Clear()
{
	this->length = 0;
	this->write_index = 0;
	this->read_index = 0;
}

volatile uint8_t bat_en_mask = 0;

void bat_en_high(uint8_t mask)
{
	bat_en_mask |= mask;
	GpioWrite(BAT_EN, HIGH);
}

void bat_en_low(uint8_t mask)
{
	bat_en_mask &= ~mask;

	if (bat_en_mask == 0)
		GpioWrite(BAT_EN, LOW);
}


bool cmpn(char * s1, const char * s2, uint8_t n)
{
	for (uint8_t i = 0; i < n; i++)
	{
		if (s1[i] != s2[i])
			return false;
	}
	return true;
}

bool cmpn_p(char * s1, const char * s2, uint8_t n)
{
	for (uint8_t i = 0; i < n; i++)
	{
		if (s1[i] != pgm_read_byte(&s2[i]))
			return false;
	}
	return true;
}

int freeRam()
{
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

bool LoadEEPROM()
{
	FILINFO fno;

	if (f_stat("UPDATE.EE", &fno) == FR_OK)
	{
		DEBUG("EE update found.\n");

		FIL * ee_file;
		ee_file = new FIL;

		f_open(ee_file, "UPDATE.EE", FA_READ);
		uint16_t rd = 0;

		byte4 tmp;

		f_read(ee_file, tmp.uint8, sizeof(tmp), &rd);
		if (tmp.uint32 != BUILD_NUMBER)
		{
			gui_showmessage_P(PSTR("UPDATE.EE is not\ncompatibile!"));
			DEBUG("Rejecting update file %lu != %lu\n", tmp.uint32, BUILD_NUMBER);
			delete ee_file;
			return false;
		}

		//rewind the file
		f_lseek(ee_file, 0);
		DEBUG("tell = %d\n", f_tell(ee_file));

		for (uint16_t i = 0; i < fno.fsize; i += rd)
		{
			uint8_t buf[256];

			f_read(ee_file, buf, sizeof(buf), &rd);

			DEBUG("tell = %d\n", f_tell(ee_file));
			DEBUG(" %d / %d\n", i + rd, fno.fsize);

			eeprom_busy_wait();
			eeprom_update_block(buf, (uint8_t *)(APP_INFO_EE_offset + i), rd);
		}

		gui_showmessage_P(PSTR("UPDATE.EE\napplied."));
		delete ee_file;
		return true;
	}
	return false;
}

bool StoreEEPROM()
{
	wdt_reset();
	DEBUG("Storing settings\n");

	FIL * ee_file;
	ee_file = new FIL;

	if (f_open(ee_file, "CFG.EE", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		DEBUG("Unable to create file\n");
		return false;
		delete ee_file;
	}
	uint16_t wd = 0;

	eeprom_busy_wait();
	eeprom_update_dword(&config_ee.build_number, BUILD_NUMBER);

	uint16_t res;

	uint16_t i = 0;
	do
	{
		uint8_t buf[256];
		uint16_t rwd;

		if (i + sizeof(buf) < sizeof(cfg_t))
			wd = sizeof(buf);
		else
			wd = sizeof(cfg_t) - i;

		DEBUG(" >> i %d s %d rwd %d\n", i, wd, rwd);

		eeprom_busy_wait();
		eeprom_read_block(buf, (uint8_t *)(APP_INFO_EE_offset + i), wd);

		DEBUG(" << \n");

		res = f_write(ee_file, buf, wd, &rwd);

		i += wd;
		DEBUG("i %d s %d rwd %d\n", i, wd, rwd);
	} while (i < sizeof(cfg_t));

	f_close(ee_file);
	DEBUG("File closed\n");

	delete ee_file;
	return true;
}

uint8_t flip_table[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

uint8_t fast_flip(uint8_t in)
{
	uint8_t out = flip_table[0x0F & in] << 4;
	out |= flip_table[(0xF0 & in) >> 4];
	return out;
}

