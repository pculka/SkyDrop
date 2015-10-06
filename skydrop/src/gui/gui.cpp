#include "gui.h"

#include "../drivers/audio/sequencer.h"
#include "../drivers/bluetooth/bt.h"

#include "widgets/widgets.h"

#include "pages.h"
#include "splash.h"
#include "settings/settings.h"
#include "settings/set_vario.h"
#include "settings/set_audio.h"
#include "gui_value.h"
#include "settings/set_widgets.h"
#include "settings/layouts.h"
#include "settings/set_layout.h"
#include "settings/set_display.h"
#include "usb.h"
#include "factory_test.h"
#include "settings/set_system.h"
#include "settings/set_autostart.h"
#include "settings/set_gps.h"
#include "settings/set_gps_detail.h"
#include "settings/set_debug.h"
#include "settings/set_altimeters.h"
#include "settings/set_altimeter.h"
#include "settings/set_time.h"
#include "settings/set_logger.h"
#include "gui_dialog.h"
#include "settings/set_bluetooth.h"
#include "update.h"
#include "settings/set_weaklift.h"
#include "settings/set_menu_audio.h"


n5110display disp;
CreateStdOut(lcd_out, disp.Write);

volatile uint8_t gui_task = GUI_NONE;
volatile uint8_t gui_new_task = GUI_SPLASH;

void (* gui_init_array[])() =
	{gui_pages_init, gui_settings_init, gui_splash_init, gui_set_vario_init, gui_value_init,
	gui_set_audio_init, gui_set_widgets_init, gui_layouts_init, gui_set_layout_init,
	gui_set_display_init, gui_usb_init, gui_factory_test_init, gui_set_system_init,
	gui_set_autostart_init, gui_set_gps_init, gui_set_gps_detail_init, gui_set_debug_init,
	gui_set_altimeters_init, gui_set_altimeter_init, gui_set_time_init, gui_set_logger_init,
	gui_dialog_init, gui_set_bluetooth_init, gui_update_init, gui_set_weaklift_init,
	gui_set_menu_audio_init};

void (* gui_stop_array[])() =
	{gui_pages_stop, gui_settings_stop, gui_splash_stop, gui_set_vario_stop, gui_value_stop,
	gui_set_audio_stop, gui_set_widgets_stop, gui_layouts_stop, gui_set_layout_stop,
	gui_set_display_stop, gui_usb_stop, gui_factory_test_stop, gui_set_system_stop,
	gui_set_autostart_stop, gui_set_gps_stop, gui_set_gps_detail_stop, gui_set_debug_stop,
	gui_set_altimeters_stop, gui_set_altimeter_stop, gui_set_time_stop, gui_set_logger_stop,
	gui_dialog_stop, gui_set_bluetooth_stop, gui_update_stop, gui_set_weaklift_stop,
	gui_set_menu_audio_stop};

void (* gui_loop_array[])() =
	{gui_pages_loop, gui_settings_loop, gui_splash_loop, gui_set_vario_loop, gui_value_loop,
	gui_set_audio_loop, gui_set_widgets_loop, gui_layouts_loop, gui_set_layout_loop,
	gui_set_display_loop, gui_usb_loop, gui_factory_test_loop, gui_set_system_loop,
	gui_set_autostart_loop, gui_set_gps_loop, gui_set_gps_detail_loop, gui_set_debug_loop,
	gui_set_altimeters_loop, gui_set_altimeter_loop, gui_set_time_loop, gui_set_logger_loop,
	gui_dialog_loop, gui_set_bluetooth_loop, gui_update_loop, gui_set_weaklift_loop,
	gui_set_menu_audio_loop};

void (* gui_irqh_array[])(uint8_t type, uint8_t * buff) =
	{gui_pages_irqh, gui_settings_irqh, gui_splash_irqh, gui_set_vario_irqh, gui_value_irqh,
	gui_set_audio_irqh, gui_set_widgets_irqh, gui_layouts_irqh, gui_set_layout_irqh,
	gui_set_display_irqh, gui_usb_irqh, gui_factory_test_irqh, gui_set_system_irqh,
	gui_set_autostart_irqh, gui_set_gps_irqh, gui_set_gps_detail_irqh, gui_set_debug_irqh,
	gui_set_altimeters_irqh, gui_set_altimeter_irqh, gui_set_time_irqh, gui_set_logger_irqh,
	gui_dialog_irqh, gui_set_bluetooth_irqh, gui_update_irqh, gui_set_weaklift_irqh,
	gui_set_menu_audio_irqh};

#define GUI_ANIM_STEPS	20

MK_SEQ(snd_but_short, ARR({1000}), ARR({50}));
MK_SEQ(snd_but_long, ARR({800}), ARR({200}));

void gui_switch_task(uint8_t new_task)
{
	gui_new_task = new_task;
}


uint32_t lcd_brightness_end = 0;

uint8_t lcd_contrast_min;
uint8_t lcd_contrast_max;

volatile bool gui_buttons_override = false;

volatile bool lcd_new_cfg = false;

void gui_trigger_backlight()
{
	if (config.gui.brightness == 0 || config.gui.brightness_timeout == 0)
		lcd_bckl(0);
	else
	{
		lcd_bckl(config.gui.brightness);
		lcd_brightness_end = task_get_ms_tick() + config.gui.brightness_timeout * 1000;
	}
}

void gui_change_disp_cfg()
{
	lcd_new_cfg = true;
}

char gui_message_line1[20];
char gui_message_line2[20];
uint32_t gui_message_end = 0;
#define MESSAGE_DURATION	5

void gui_showmessage_P(const char * msg)
{
	char tmp[40];
	strcpy_P(tmp, msg);
	gui_showmessage(tmp);
}

void gui_showmessage(char * msg)
{
	char * ptr;
	if ((ptr = strchr(msg, '\n')) != NULL)
	{
		memcpy(gui_message_line1, msg, ptr - msg + 1);
		gui_message_line1[ptr - msg] = 0;
		strcpy(gui_message_line2, ptr + 1);
	}
	else
	{
		strcpy(gui_message_line1, msg);
		gui_message_line2[0] = 0;
	}
	gui_message_end = task_get_ms_tick() + MESSAGE_DURATION * 1000ul;
}

void gui_raligh_text_P(const char * text, uint8_t x, uint8_t y)
{
	char tmp[16];
	strcpy_P(tmp, text);
	gui_raligh_text(tmp, x, y);
}

void gui_raligh_text(char * text, uint8_t x, uint8_t y)
{
	disp.GotoXY(x - disp.GetTextWidth(text), y);
	fprintf_P(lcd_out, PSTR("%s"), text);
}

void gui_caligh_text_P(const char * text, uint8_t x, uint8_t y)
{
	char tmp[16];
	strcpy_P(tmp, text);
	gui_caligh_text(tmp, x, y);
}

void gui_caligh_text(char * text, uint8_t x, uint8_t y)
{
	disp.GotoXY(x - disp.GetTextWidth(text) / 2, y);
	fprintf_P(lcd_out, PSTR("%s"), text);
}


void gui_init()
{
	disp.Init();
	gui_load_eeprom();
}

void gui_load_eeprom()
{
	eeprom_busy_wait();

	if (config.gui.brightness == 0xFF)
		config.gui.brightness = 100;

	if (config.gui.brightness_timeout == 0xFF)
		config.gui.brightness_timeout = 3;

	if (config.gui.contrast > GUI_CONTRAST_STEPS)
		config.gui.contrast = GUI_CONTRAST_STEPS / 2;

	lcd_contrast_min = eeprom_read_byte(&config_ro.lcd_contrast_min);
	if (lcd_contrast_min == 0xFF)
		lcd_contrast_min = 16;

	lcd_contrast_max = eeprom_read_byte(&config_ro.lcd_contrast_max);
	if (lcd_contrast_max == 0xFF)
		lcd_contrast_max = 127;

	DEBUG("lcd_contrast_min %d\n", lcd_contrast_min);
	DEBUG("lcd_contrast_max %d\n", lcd_contrast_max);
	DEBUG("lcd_contrast %d\n", config.gui.contrast);

	if (config.gui.disp_flags == 0xFF)
		config.gui.disp_flags = CFG_DISP_ANIM;

	disp.SetFlip(config.gui.disp_flags & CFG_DISP_FLIP);
	gui_change_disp_cfg();
	gui_update_disp_cfg();
}

void gui_stop()
{
	disp.Stop();
}

uint8_t fps_counter = 0;
uint8_t fps_val = 0;
uint32_t fps_timer = 0;

uint32_t gui_loop_timer = 0;

void gui_dialog_P(const char * title)
{
	char tmp[16];
	strcpy_P(tmp, title);
	gui_dialog(tmp);
}

void gui_dialog(char * title)
{
	disp.LoadFont(F_TEXT_M);
	gui_caligh_text(title, GUI_DISP_WIDTH / 2, 2);

	disp.InvertPart(0, 0, n5110_width, 0);
	disp.PutPixel(0, 0, 0);
	disp.PutPixel(n5110_width - 1, 0 ,0);
	disp.Invert(0, 8, n5110_width, 11);

	disp.DrawLine(0, 12, 0, n5110_height - 2, 1);
	disp.DrawLine(n5110_width - 1, 12, n5110_width - 1, n5110_height - 2, 1);
	disp.DrawLine(1, n5110_height - 1, n5110_width - 2, n5110_height - 1, 1);
}

void gui_update_disp_cfg()
{
	if (lcd_new_cfg)
	{
		//need to be outside of IRQ
		lcd_new_cfg = false;

		DEBUG(" ** gui_update_disp_cfg **\n");
		DEBUG("lcd_contrast %d\n", config.gui.contrast);


		uint8_t new_contrast = lcd_contrast_min + ((lcd_contrast_max - lcd_contrast_min) * config.gui.contrast) / GUI_CONTRAST_STEPS;

		DEBUG("new_contrast %d\n", new_contrast);

		disp.SetContrast(new_contrast);
		disp.SetInvert(config.gui.disp_flags & CFG_DISP_INVERT);
	}
}

//disp layers
// 0 - layer to draw
// 1 - help layer
void gui_loop()
{
	if (gui_loop_timer > task_get_ms_tick())
		return;

	gui_loop_timer = (uint32_t)task_get_ms_tick() + (uint32_t)50; //20 fps

	gui_update_disp_cfg();

	if (gui_new_task != gui_task)
	{
		if (gui_task != GUI_NONE)
			gui_stop_array[gui_task]();

		gui_task = gui_new_task;
		buttons_reset();
		gui_init_array[gui_task]();
	}

	disp.ClearBuffer();

	gui_loop_array[gui_task]();

	if (gui_message_end > task_get_ms_tick())
	{
		disp.LoadFont(F_TEXT_M);
		uint8_t w = disp.GetTextWidth(gui_message_line1);
		uint8_t h = disp.GetAHeight();

		if (gui_message_line2[0] != 0)
		{
			uint8_t tmp = disp.GetTextWidth(gui_message_line2);
			if (tmp > w)
				w = tmp;
			h += disp.GetTextHeight();
		}

		uint8_t x = GUI_DISP_WIDTH / 2 - w / 2;
		uint8_t y = GUI_DISP_HEIGHT / 2 - h / 2;

		uint8_t pad = 3;
		disp.DrawRectangle(x - 1 - pad, y - 1 - pad, x + w + pad, y + h + pad, 0, 1);

		disp.DrawLine(x - pad,			y - 2 - pad, 		x + w + pad, 		y - 2 - pad, 		1);
		disp.DrawLine(x - pad, 			y + h + 1 + pad, 	x + w + pad, 		y + h + 1 + pad, 	1);

		disp.DrawLine(x - 1 - pad, 		y - 1 - pad, 		x - 1 - pad, 		y + h + pad, 	1);
		disp.DrawLine(x + w + 1 + pad, 	y - 1 - pad, 		x + w + 1 + pad, 	y + h + pad, 	1);
		disp.GotoXY(x, y);
		fprintf_P(lcd_out, PSTR("%s"), gui_message_line1);

		if (gui_message_line2[0] != 0)
		{
			disp.GotoXY(x, y + disp.GetTextHeight());
			fprintf_P(lcd_out, PSTR("%s"), gui_message_line2);
		}
	}

	// FPS counter

//	fps_counter++;
//
//	if (fps_timer < task_get_ms_tick())
//	{
//		fps_val = fps_counter;
//		fps_counter = 0;
//		fps_timer = task_get_ms_tick() + 1000;
//	}
//
//	disp.LoadFont(font_6px_normal_ttf_8);
//	disp.GotoXY(1, 1);
//	disp.ClearPart(0, 0, 1, 10);
//	fprintf_P(lcd_out, PSTR("%d"), fps_val);

	// FPS end

	disp.Draw();

	if (gui_task != GUI_SPLASH)
		if (buttons_read(B_LEFT) || buttons_read(B_RIGHT) || buttons_read(B_MIDDLE))
			gui_trigger_backlight();

	if (lcd_brightness_end < task_get_ms_tick())
		lcd_bckl(0);
}

void gui_force_loop()
{
	disp.ClearBuffer();

	gui_update_disp_cfg();

	if (gui_new_task != gui_task)
	{
		if (gui_task != GUI_NONE)
			gui_stop_array[gui_task]();
		gui_task = gui_new_task;
		gui_init_array[gui_task]();
	}

	gui_loop_array[gui_task]();

	disp.Draw();
}

void gui_irqh(uint8_t type, uint8_t * buff)
{
	if (type == B_LEFT || type == B_MIDDLE || type == B_RIGHT)
	{
		if (config.gui.menu_audio_flags & CFG_AUDIO_MENU_BUTTONS && gui_buttons_override == false)
		{
			if (*buff == BE_CLICK)
				seq_start(&snd_but_short, config.gui.menu_volume);
			if (*buff == BE_LONG)
				seq_start(&snd_but_long, config.gui.menu_volume);
		}

		if (gui_message_end > task_get_ms_tick())
		{
			if (type == B_MIDDLE && *buff == BE_CLICK)
				gui_message_end = 0;

			return;
		}
	}

	switch(type)
	{

	default:
		if (gui_task != GUI_NONE)
			gui_irqh_array[gui_task](type, buff);
	}
}

void gui_statusbar()
{
	//GPS indicator
	if (config.system.use_gps)
	{
		char tmp[3];
		disp.LoadFont(F_TEXT_S);
		sprintf_P(tmp, PSTR("G"));

		if(fc.gps_data.valid)
		{
			gui_raligh_text(tmp, GUI_DISP_WIDTH - 1, 1);
		}
		else
		{
			if (GUI_BLINK_TGL(1000))
				gui_raligh_text(tmp, GUI_DISP_WIDTH - 1, 1);
		}
	}

	//BT indicator
	if (config.system.use_bt)
	{
		char tmp[3];
		disp.LoadFont(F_TEXT_S);
		sprintf_P(tmp, PSTR("B"));

		if(bt_device_active())
		{
			gui_raligh_text(tmp, GUI_DISP_WIDTH - 1, 10);
		}
		else
		{
			if (GUI_BLINK_TGL(1000))
				gui_raligh_text(tmp, GUI_DISP_WIDTH - 1, 10);
		}
	}


	//battery indicator
	uint8_t a = battery_per / 10;

	disp.DrawLine(GUI_DISP_WIDTH - 5, GUI_DISP_HEIGHT - 13, GUI_DISP_WIDTH - 2, GUI_DISP_HEIGHT - 13, 1);
	disp.DrawRectangle(GUI_DISP_WIDTH - 6, GUI_DISP_HEIGHT - 12, GUI_DISP_WIDTH - 1, GUI_DISP_HEIGHT - 1, 1, 0);
	disp.DrawRectangle(GUI_DISP_WIDTH - 5, GUI_DISP_HEIGHT - 1 - a, GUI_DISP_WIDTH - 2, GUI_DISP_HEIGHT - 1, 1, 1);

}
