#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "onboard-led.h"
#include "htu21.h"
#include "batt_serv.h"
#include "rtc.h"


struct rh_ctx {
	struct service_desc;
	struct char_desc rh;
	uint8_t last_reading;
};

struct temp_ctx {
	struct service_desc;
	struct char_desc temp;
	int8_t last_reading;
};


static bool
rh_reading(struct rh_ctx *ctx)
{
	return htu21_read_humidity(&ctx->last_reading);
}

static void
rh_char_srv_update(struct rh_ctx *ctx)
{
	if (rh_reading(ctx)) {
		simble_srv_char_update(&ctx->rh, &ctx->last_reading);
	}
}

static void
rh_connected(struct service_desc *s)
{
	rh_char_srv_update((struct rh_ctx *) s);
}

static void
rh_read_cb(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len)
{
	struct rh_ctx *ctx = (struct rh_ctx *) s;
	if (rh_reading(ctx)) {
		*len = 1;
		*val = &ctx->last_reading;
	}
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
		ORG_BLUETOOTH_UNIT_PERCENTAGE);
	ctx->connect_cb = rh_connected;
	ctx->rh.read_cb = rh_read_cb;
	ctx->rh.notify = 1;
	simble_srv_register(ctx);
}

static bool
temp_reading(struct temp_ctx *ctx)
{
	return htu21_read_temperature(&ctx->last_reading);
}

static void
temp_char_srv_update(struct temp_ctx *ctx)
{
	if (temp_reading(ctx)) {
		simble_srv_char_update(&ctx->temp, &ctx->last_reading);
	}
}

static void
temp_connected(struct service_desc *s)
{
	temp_char_srv_update((struct temp_ctx *) s);
}

static void
temp_read_cb(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len)
{
	struct temp_ctx *ctx = (struct temp_ctx *) s;
	if (temp_reading(ctx)) {
		*len = 1;
		*val = &ctx->last_reading;
	}
}

static void
temp_init(struct temp_ctx *ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_TEMP_SERVICE);
	simble_srv_char_add(ctx, &ctx->temp,
		simble_get_vendor_uuid_class(), VENDOR_UUID_TEMP_CHAR,
		u8"Temperature",
		1);
	simble_srv_char_attach_format(&ctx->temp,
		BLE_GATT_CPF_FORMAT_SINT8,
		0,
		ORG_BLUETOOTH_UNIT_DEGREE_CELSIUS);
	ctx->connect_cb = temp_connected;
	ctx->temp.read_cb = temp_read_cb;
	ctx->temp.notify = 1;
	simble_srv_register(ctx);
}


static struct rh_ctx rh_ctx;
static struct temp_ctx temp_ctx;


static void
notif_timer_cb(struct rtc_ctx *ctx)
{
  simble_srv_char_notify(&temp_ctx.temp, false, 1, &temp_ctx.last_reading);
	simble_srv_char_notify(&rh_ctx.temp, false, 1, &rh_ctx.last_reading);
}

void
main(void)
{
	twi_master_init();

	simble_init("Temperature/RH");

	my_service_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
  //Set the timer parameters and initialize it.
  struct rtc_ctx rtc_ctx = {
      .rtc_x[NOTIF_TIMER_ID] = {.type = PERIODIC,
                                .period = my_service_ctx.sampling_period,
                                .enabled = false,
                                .cb = notif_timer_cb,
      }
  };

	// NOTE: rtc_init needs to be called AFTER simble_init which leaves
	//		the SoftDevice to configure the LFCLKSRC to the external XTAL
  rtc_init(&rtc_ctx);


	ind_init();
	batt_serv_init();
	rh_init(&rh_ctx);
	temp_init(&temp_ctx);
	simble_adv_start();
	simble_process_event_loop();
}
