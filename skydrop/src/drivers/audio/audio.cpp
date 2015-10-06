#include "audio.h"
#include "../../fc/fc.h"

#include "buzzer.h"
#include "sequencer.h"
#include "vario.h"

Timer audio_timer;


//demo
volatile bool audio_demo = false;
volatile float audio_demo_val = 0;

void audio_init()
{
	AUDIO_TIMER_PWR_ON;
	audio_timer.Init(AUDIO_TIMER, timer_div1024);
	audio_timer.EnableInterrupts(timer_overflow);
}

void audio_step()
{
	//sound effect is high priority
	if (seq_enabled)
	{
		seq_loop();
		return;
	}

	//vario demo sound suppress standard vario sound
	if (audio_demo)
	{
		audio_vario_step(audio_demo_val);
		return;
	}

	//barometer data are valid now
	if (fc.baro_valid)
	{
		//audio is suppressed due auto start
		if (config.autostart.supress_audio)
		{
			//vario in flight -> enable sound
			if (fc.autostart_state == AUTOSTART_FLIGHT)
				audio_vario_step(fc.vario);

			return;
		}

		//audio suppress is not used
		audio_vario_step(fc.vario);
	}
}

void audio_off()
{
	//stop unused timer
	audio_timer.Stop();
	//reset state of audio vario
	audio_vario_reset();
	//silence!
	buzzer_set_vol(0);
}


