#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "rtc.h"

struct ir_protocol_logical {
	uint8_t ticks; // a "zero" corresponds to 1 (1125us) tick
	uint8_t pulses;
};

struct ir_protocol {
	uint8_t pulse_width; // = 1 / carrier frequency (e.g. 1 / 38kHz = ~26us)
	uint16_t tick_freq; // Hz = 1 / tick time (e.g. 1 / 1125us = ~889Hz)
	struct {
		uint8_t length; // bits (e.g. 8)
		uint8_t send_complement;
	} address;
		struct {
		uint8_t length; // bits (e.g. 8)
		uint8_t send_complement;
	} command;
	enum {
		IR_PROTOCOL_MODULATION_PULSE_DISTANCE = 0x0,
		// IR_PROTOCOL_MODULATION_MANCHESTER = 0x1, // TODO
	} modulation;
	struct {
		uint16_t leader; // us
		uint16_t pause; // us
	} preamble;
	struct ir_protocol_logical zero;
	struct ir_protocol_logical one;
};

typedef void (sent_cb_t)(uint16_t address, uint16_t command);

void protocol_init(struct ir_protocol *protocol, uint8_t led_pin, struct rtc_ctx *c);
bool protocol_send(uint16_t address, uint16_t command, sent_cb_t* cb);

#endif /* PROTOCOL_H */
