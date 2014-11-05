#include <stdlib.h>
#include <string.h>
#include <nrf_delay.h>

#include "simble.h"
#include "indicator.h"
#include "protocol.h"


struct ir_ctx {
	struct service_desc;
	struct char_desc transmitter;
};

enum ir_pin {
	IR_PIN_OUT = 25
};

struct ir_payload {
	uint16_t address;
	uint16_t command;
};

static struct ir_protocol nec = {
	.pulse_width = 26,
	.tick_freq = 889,
	.modulation = IR_PROTOCOL_MODULATION_PULSE_DISTANCE,
	.preamble = {
		.leader = 9000,
		.pause = 4500,
	},
	.address = {
		.length = 8,
		.send_complement = 1,
	},
	.command = {
		.length = 8,
		.send_complement = 1,
	},
	.zero = {
		.ticks = 1,
		.pulses = 22,
	},
	.one = {
		.ticks = 2,
		.pulses = 22,
	},
};

static struct ir_ctx ir_ctx;

static void
ir_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
	struct ir_payload *payload = (struct ir_payload*) val;
	nrf_delay_us(50 * 999); // XXX delay ~50ms to avoid interrupt screw up
	protocol_send(payload->address, payload->command, NULL);
}

static void
ir_init(struct ir_ctx* ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	simble_srv_char_add(ctx, &ctx->transmitter,
		simble_get_vendor_uuid_class(), VENDOR_UUID_IR_CHAR,
		u8"transmitter",
		4);
	simble_srv_char_attach_format(&ctx->transmitter,
		BLE_GATT_CPF_FORMAT_UINT32,
		0,
		ORG_BLUETOOTH_UNIT_UNITLESS);
	ctx->transmitter.write_cb = ir_write_cb;
	simble_srv_register(ctx);
}

void
main(void)
{
	simble_init("IR transmitter");
	protocol_init(&nec, IR_PIN_OUT);
	ind_init();
	ir_init(&ir_ctx);
	simble_adv_start();
	simble_process_event_loop();
}
