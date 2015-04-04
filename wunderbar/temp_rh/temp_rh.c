#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "onboard-led.h"
#include "htu21.h"
#include "batt_serv.h"
#include "rtc.h"
#include "i2c.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN_SAMPLING_PERIOD 250UL

#define NOTIF_TIMER_ID  0

struct rh_ctx {
	struct service_desc;
	struct char_desc rh;
	struct char_desc sampling_period_rh;
	uint8_t last_reading;
	uint32_t sampling_period;
};

struct temp_ctx {
	struct service_desc;
	struct char_desc temp;
	struct char_desc sampling_period_temp;
	int8_t last_reading;
	uint32_t sampling_period;
};

static struct rh_ctx rh_ctx;
static struct temp_ctx temp_ctx;


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
rh_disconnected(struct service_desc *s)
{
	rtc_update_cfg(rh_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
	rtc_update_cfg(temp_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID+1, false);
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
rh_sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct rh_ctx *ctx = (struct rh_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
rh_sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct rh_ctx *ctx = (struct rh_ctx *)s;
	if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
		ctx->sampling_period = *(uint32_t*)val;
	else
		ctx->sampling_period = MIN_SAMPLING_PERIOD;
	rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
rh_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct rh_ctx *ctx = (struct rh_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
rh_notif_timer_cb(struct rtc_ctx *ctx)
{
	void *val = &rh_ctx.last_reading;
	uint16_t len = sizeof(rh_ctx.last_reading);
	rh_read_cb(&rh_ctx, &rh_ctx.rh, &val, &len);
	simble_srv_char_notify(&rh_ctx.rh, false, len, val);
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
	simble_srv_char_add(ctx, &ctx->sampling_period_rh,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(ctx->sampling_period)); // size in bytes
	ctx->connect_cb = rh_connected;
	ctx->disconnect_cb = rh_disconnected;
	ctx->rh.read_cb = rh_read_cb;
	ctx->rh.notify = 1;
	ctx->rh.notify_status_cb = rh_notify_status_cb;
	ctx->sampling_period_rh.read_cb = rh_sampling_period_read_cb;
	ctx->sampling_period_rh.write_cb = rh_sampling_period_write_cb;
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
temp_disconnected(struct service_desc *s)
{
	rtc_update_cfg(temp_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);}

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
temp_sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct temp_ctx *ctx = (struct temp_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
temp_sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct temp_ctx *ctx = (struct temp_ctx *)s;
	if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, true);
}


void
temp_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct temp_ctx *ctx = (struct temp_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, false);
}


static void
temp_init(struct temp_ctx *ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE_2);
	simble_srv_char_add(ctx, &ctx->temp,
		simble_get_vendor_uuid_class(), VENDOR_UUID_TEMP_CHAR,
		u8"Temperature",
		1);
	simble_srv_char_attach_format(&ctx->temp,
		BLE_GATT_CPF_FORMAT_SINT8,
		0,
		ORG_BLUETOOTH_UNIT_DEGREE_CELSIUS);
	simble_srv_char_add(ctx, &ctx->sampling_period_temp,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_temp,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
	ctx->connect_cb = temp_connected;
	ctx->disconnect_cb = temp_disconnected;
	ctx->temp.read_cb = temp_read_cb;
	ctx->temp.notify = 1;
	ctx->temp.notify_status_cb = temp_notify_status_cb;
        ctx->sampling_period_temp.read_cb = temp_sampling_period_read_cb;
	ctx->sampling_period_temp.write_cb = temp_sampling_period_write_cb;
	simble_srv_register(ctx);
}

static void
temp_notif_timer_cb(struct rtc_ctx *ctx)
{
	void *val = &temp_ctx.last_reading;
	uint16_t len = sizeof(temp_ctx.last_reading);
	temp_read_cb(&temp_ctx, &temp_ctx.temp, &val, &len);
	simble_srv_char_notify(&temp_ctx.temp, false, len, val);
}

void
main(void)
{
	twi_master_init();

	simble_init("Temperature/RH");

	rh_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
	temp_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
	//Set the timer parameters and initialize it.
	struct rtc_ctx rtc_ctx = {
		.rtc_x[NOTIF_TIMER_ID] = {
			.type = PERIODIC,
                        .period = rh_ctx.sampling_period,
                        .enabled = false,
                        .cb = rh_notif_timer_cb,
		},
		.rtc_x[NOTIF_TIMER_ID+1] = {
			.type = PERIODIC,
                        .period = temp_ctx.sampling_period,
                        .enabled = false,
                        .cb = temp_notif_timer_cb,
		}
	};
	batt_serv_init(&rtc_ctx);
	rtc_init(&rtc_ctx);

	ind_init();
	rh_init(&rh_ctx);
	temp_init(&temp_ctx);
	simble_adv_start();

	simble_process_event_loop();
}
