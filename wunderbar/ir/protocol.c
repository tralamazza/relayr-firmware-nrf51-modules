#include <stdbool.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_gpiote.h>
#include <nrf_soc.h>

#include "protocol.h"
#include "util.h"

#define RTC_TASK_JITTER		(30u)
#define LFCLK_FREQUENCY		(32768ul)

static struct {
	uint8_t led_pin;
	struct ir_protocol *protocol;
	enum {
		PROTOCOL_STATE_IDLE = 0x0,
		PROTOCOL_STATE_PREAMBLE_DONE,
		PROTOCOL_STATE_ADDRESS,
		PROTOCOL_STATE_COMMAND,
		PROTOCOL_STATE_LAST_BIT,
		PROTOCOL_STATE_END,
	} state;
	uint8_t bit_position;
	uint8_t invert;
	uint16_t address;
	uint16_t command;
	sent_cb_t* cb;
} context = {
	.state = PROTOCOL_STATE_IDLE,
	.bit_position = 0,
	.invert = 0,
	.address = 0,
	.command = 0,
};

static inline void
pulse(uint32_t num)
{
	NRF_TIMER2->CC[0] = num;
	NRF_TIMER1->TASKS_START = 1;
}

static void
clock_bit_position(uint16_t data)
{
	uint8_t bit = (data & (1 << context.bit_position)) > 0;
	struct ir_protocol_logical *logical = (bit ^ context.invert) ?
		&context.protocol->one : &context.protocol->zero;
	NRF_RTC1->CC[0] += logical->ticks;
	pulse(logical->pulses);
}

void
RTC1_IRQHandler(void)
{
	if (NRF_RTC1->EVENTS_COMPARE[0] == 0)
		return;
	NRF_RTC1->EVENTS_COMPARE[0] = 0;
	switch (context.state) {
	case PROTOCOL_STATE_IDLE:
		/* should not happen */
		break;
	case PROTOCOL_STATE_PREAMBLE_DONE:
		NRF_RTC1->TASKS_STOP = 1;
		NRF_RTC1->PRESCALER = ROUNDED_DIV(LFCLK_FREQUENCY, context.protocol->tick_freq) - 1;
		context.state = PROTOCOL_STATE_ADDRESS;
		context.bit_position = 0;
		NRF_RTC1->TASKS_START = 1;
		/* FALLTHROUGH */
	case PROTOCOL_STATE_ADDRESS:
		clock_bit_position(context.address);
		if (++context.bit_position == context.protocol->address.length) {
			context.bit_position = 0;
			if (context.protocol->address.send_complement) {
				if (context.invert == 0) {
					context.invert = 1;
				} else {
					context.invert = 0;
					context.state = PROTOCOL_STATE_COMMAND;
				}
			} else {
				context.state = PROTOCOL_STATE_COMMAND;
			}
		}
		break;
	case PROTOCOL_STATE_COMMAND:
		clock_bit_position(context.command);
		if (++context.bit_position == context.protocol->command.length) {
			context.bit_position = 0;
			if (context.protocol->command.send_complement) {
				if (context.invert == 0) {
					context.invert = 1;
				} else {
					context.invert = 0;
					context.state = PROTOCOL_STATE_LAST_BIT;
				}
			} else {
				context.state = PROTOCOL_STATE_LAST_BIT;
			}
		}
		break;
	case PROTOCOL_STATE_LAST_BIT:
		clock_bit_position(context.command);
		context.state = PROTOCOL_STATE_END;
		break;
	case PROTOCOL_STATE_END:
		NRF_RTC1->TASKS_STOP = 1;
		NRF_RTC1->TASKS_CLEAR = 1;
		NRF_POWER->TASKS_LOWPWR = 1; // PAN 11 "HFCLK: Base current with HFCLK running is too high"
		context.state = PROTOCOL_STATE_IDLE;
		if (context.cb) {
			context.cb(context.address, context.command);
		}
		break;
	}
}

void
protocol_init(struct ir_protocol *protocol, uint8_t led_pin)
{
	context.protocol = protocol;
	context.led_pin = led_pin;

	// low freq clock
	NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
		/* NOTHING */;
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

	// rtc1 interrupt
	sd_nvic_ClearPendingIRQ(RTC1_IRQn);
	sd_nvic_SetPriority(RTC1_IRQn, NRF_APP_PRIORITY_LOW);
	sd_nvic_EnableIRQ(RTC1_IRQn);
	NRF_RTC1->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
	NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk;

	// high freq clock
	sd_clock_hfclk_request();

	// timer1
	NRF_TIMER1->TASKS_STOP = 1;
	NRF_TIMER1->TASKS_CLEAR = 1;
	NRF_TIMER1->PRESCALER = 4;
	NRF_TIMER1->MODE = TIMER_MODE_MODE_Timer;
	NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
	NRF_TIMER1->SHORTS = TIMER_SHORTS_COMPARE2_CLEAR_Msk;
	NRF_TIMER1->CC[0] = 1;
	NRF_TIMER1->CC[1] = ROUNDED_DIV(context.protocol->pulse_width, 3);
	NRF_TIMER1->CC[2] = context.protocol->pulse_width;

	// timer2 (counter)
	NRF_TIMER2->TASKS_STOP = 1;
	NRF_TIMER2->TASKS_CLEAR = 1;
	NRF_TIMER2->MODE = TIMER_MODE_MODE_Counter;
	NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
	NRF_TIMER2->TASKS_START = 1;
	NRF_TIMER2->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;

	// gpio (led)
	nrf_gpio_cfg_output(led_pin);

	// gpiote0 (toggles gpio)
	nrf_gpiote_task_config(0, led_pin, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);

	// ppi's
	sd_ppi_channel_assign(0, &NRF_TIMER1->EVENTS_COMPARE[0], &NRF_GPIOTE->TASKS_OUT[0]); // toggle led
	sd_ppi_channel_assign(1, &NRF_TIMER1->EVENTS_COMPARE[1], &NRF_GPIOTE->TASKS_OUT[0]); // toggle led
	sd_ppi_channel_assign(2, &NRF_TIMER1->EVENTS_COMPARE[2], &NRF_TIMER2->TASKS_COUNT); // inc timer2
	sd_ppi_channel_assign(3, &NRF_TIMER2->EVENTS_COMPARE[0], &NRF_TIMER1->TASKS_STOP); // stops timer1 after timer2 reaches N
	sd_ppi_channel_enable_set(PPI_CHEN_CH0_Msk |
		PPI_CHEN_CH1_Msk |
		PPI_CHEN_CH2_Msk |
		PPI_CHEN_CH3_Msk);
}

bool
protocol_send(uint16_t address, uint16_t command, sent_cb_t* cb)
{
	if (context.state != PROTOCOL_STATE_IDLE) {
		return false;
	}
	context.state = PROTOCOL_STATE_PREAMBLE_DONE;
	context.address = address;
	context.command = command;
	context.cb = cb;
	NRF_POWER->TASKS_CONSTLAT = 1; // PAN 11 "HFCLK: Base current with HFCLK running is too high"
	NRF_RTC1->TASKS_STOP = 1;
	NRF_RTC1->TASKS_CLEAR = 1;
	NRF_RTC1->PRESCALER = ROUNDED_DIV(LFCLK_FREQUENCY,
		ROUNDED_DIV(1000000,
			context.protocol->preamble.leader + context.protocol->preamble.pause - RTC_TASK_JITTER
			)) - 1;
	NRF_RTC1->CC[0] = 1;
	NRF_RTC1->TASKS_START = 1;
	pulse(ROUNDED_DIV(context.protocol->preamble.leader, context.protocol->pulse_width));
	return true;
}
