#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "onboard-led.h"
#include "htu21.h"


#define ORG__BLUETOOTH__UNIT__THERMODYNAMIC_TEMPERATURE__DEGREE_CELSIUS 0x272f
#define ORG__BLUETOOTH__UNIT__PERCENTAGE 0x27AD


struct rh_ctx {
	struct service_desc;
	struct char_desc rh;
};

struct temp_ctx {
	struct service_desc;
	struct char_desc temp;
};


void RTC0_IRQHandler()
{
	onboard_led(ONBOARD_LED_TOGGLE);
}

static void
rh_connected(struct service_desc *s)
{
}

static void
rh_disconnected(struct service_desc *s)
{
}

static void
rh_read_cb(struct service_desc *s, struct char_desc *c, void *val, uint16_t *len)
{
}

static void
rh_init(struct rh_ctx *ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	simble_srv_char_add(ctx, &ctx->rh,
		simble_get_vendor_uuid_class(), VENDOR_UUID_HUMID_CHAR,
		u8"Relative Humidity",
		1);
	simble_srv_char_attach_format(&ctx->rh,
		BLE_GATT_CPF_FORMAT_UINT8,
		0,
		ORG__BLUETOOTH__UNIT__PERCENTAGE);
	ctx->connect_cb = rh_connected;
	ctx->disconnect_cb = rh_disconnected;
	ctx->rh.read_cb = rh_read_cb;
	simble_srv_register(ctx);
}

static void
temp_connected(struct service_desc *s)
{
}

static void
temp_disconnected(struct service_desc *s)
{
}

static void
temp_read_cb(struct service_desc *s, struct char_desc *c, void *val, uint16_t *len)
{
}

static void
temp_init(struct temp_ctx *ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	simble_srv_char_add(ctx, &ctx->temp,
		simble_get_vendor_uuid_class(), VENDOR_UUID_TEMP_CHAR,
		u8"Temperature",
		1);
	simble_srv_char_attach_format(&ctx->temp,
		BLE_GATT_CPF_FORMAT_SINT8,
		0,
		ORG__BLUETOOTH__UNIT__THERMODYNAMIC_TEMPERATURE__DEGREE_CELSIUS);
	ctx->connect_cb = temp_connected;
	ctx->disconnect_cb = temp_disconnected;
	ctx->temp.read_cb = temp_read_cb;
	simble_srv_register(ctx);
}

static void
lfclk_init(void)
{
	NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
	}
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
}

static void
rtc_init(void)
{
	NVIC_EnableIRQ(RTC0_IRQn);
	NRF_RTC0->PRESCALER = 32767; // 1Hz, prescaler = (32768 / Hz) - 1
	NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Msk;
	NRF_RTC0->TASKS_START = 1;
}


static struct rh_ctx rh_ctx;
static struct temp_ctx temp_ctx;

void
main(void)
{
	twi_master_init();
	rtc_init();
	lfclk_init();

	simble_init("Temperature/RH");
	ind_init();
	rh_init(&rh_ctx);
	temp_init(&temp_ctx);
	simble_adv_start();
	simble_process_event_loop();
}