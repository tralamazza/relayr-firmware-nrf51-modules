#include <stdlib.h>
#include <string.h>

#include <twi_master.h>
#include <nrf_gpio.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"
#include "i2c.h"
#include "rtc.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN_SAMPLING_PERIOD 200UL

#define NOTIF_TIMER_ID  0

#include "tcs3771.h"

#define WLED_CTRL_PIN 21
#define TCS37717_INT_PIN 25

struct proximity_ctx {
        struct service_desc;
        struct char_desc proximity;
        struct char_desc sampling_period_char;
        uint16_t proximity_value;
        uint32_t sampling_period;
};

struct rgb_ctx {
        struct service_desc;
        struct char_desc rgb;
        struct char_desc sampling_period_char;
        uint64_t rgb_value;
        uint32_t sampling_period;
};

static struct proximity_ctx proximity_ctx;
static struct rgb_ctx rgb_ctx;

static void
proximity_update(struct proximity_ctx *ctx, uint16_t val)
{
        simble_srv_char_update(&ctx->proximity, &val);
}

static void
proximity_disconnected(struct service_desc *s)
{
        rtc_update_cfg(proximity_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
proximity_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct proximity_ctx *ctx = (void *)s;

        enable_i2c();
        tcs3771_init();
        while(nrf_gpio_pin_read(TCS37717_INT_PIN) == 1);
        ctx->proximity_value = tcs3771_proximity_data();
        *lenp = sizeof(ctx->proximity_value);
        *valp = &ctx->proximity_value;
        tcs3771_stop();
        disable_i2c();
}

static void
proximity_sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct proximity_ctx *ctx = (struct proximity_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(&ctx->sampling_period);
}

static void
proximity_sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct proximity_ctx *ctx = (struct proximity_ctx *)s;
        if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
proximity_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct proximity_ctx *ctx = (struct proximity_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
proximity_notif_timer_cb(struct rtc_ctx *ctx)
{
        void *val = &proximity_ctx.proximity_value;
	uint16_t len = sizeof(&proximity_ctx.proximity_value);
	proximity_read(&proximity_ctx, &proximity_ctx.proximity, &val, &len);
	simble_srv_char_notify(&proximity_ctx.proximity, false, 2,
                &proximity_ctx.proximity_value);
}

static void
proximity_init(struct proximity_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->proximity,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_PROXIMITY_CHAR,
                     u8"Proximity",
                     2);
        /* srv_char_attach_format(&ctx->proximity, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        simble_srv_char_add(ctx, &ctx->sampling_period_char,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(&ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_char,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
        ctx->disconnect_cb = proximity_disconnected;
        ctx->proximity.read_cb = proximity_read;
        ctx->proximity.notify = 1;
        ctx->proximity.notify_status_cb = proximity_notify_status_cb;
        ctx->sampling_period_char.read_cb = proximity_sampling_period_read_cb;
	ctx->sampling_period_char.write_cb = proximity_sampling_period_write_cb;
        simble_srv_register(ctx);
}


static void
rgb_update(struct rgb_ctx *ctx, uint8_t *val)
{
        simble_srv_char_update(&ctx->rgb, val);
}

static void
rgb_disconnected(struct service_desc *s)
{
        rtc_update_cfg(rgb_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID+1, false);
}

static void
rgb_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct rgb_ctx *ctx = (void *)s;

        enable_i2c();
        tcs3771_init();
        nrf_gpio_pin_write(WLED_CTRL_PIN, true);
        while(nrf_gpio_pin_read(TCS37717_INT_PIN) == 1);
        ctx->rgb_value = tcs3771_rgb_data();
        nrf_gpio_pin_write(WLED_CTRL_PIN, false);
        *lenp = sizeof(ctx->rgb_value);
        *valp = &ctx->rgb_value;
        tcs3771_stop();
        disable_i2c();
}

static void
rgb_sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct rgb_ctx *ctx = (struct rgb_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(&ctx->sampling_period);
}

static void
rgb_sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct rgb_ctx *ctx = (struct rgb_ctx *)s;
        if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, true);
}

void
rgb_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct rgb_ctx *ctx = (struct rgb_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID+1, false);
}

static void
rgb_init(struct rgb_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE_2);
        simble_srv_char_add(ctx, &ctx->rgb,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_COLOR_CHAR,
                     u8"RGB",
                     8);
        /* srv_char_attach_format(&ctx->proximity, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        simble_srv_char_add(ctx, &ctx->sampling_period_char,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(&ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_char,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
        ctx->disconnect_cb = rgb_disconnected;
        ctx->rgb.read_cb = rgb_read;
        ctx->rgb.notify = 1;
        ctx->rgb.notify_status_cb = rgb_notify_status_cb;
        ctx->sampling_period_char.read_cb = rgb_sampling_period_read_cb;
	ctx->sampling_period_char.write_cb = rgb_sampling_period_write_cb;
        simble_srv_register(ctx);
}

static void
rgb_notif_timer_cb(struct rtc_ctx *ctx)
{
        void *val = &rgb_ctx.rgb_value;
	uint16_t len = sizeof(&rgb_ctx.rgb_value);
	rgb_read(&rgb_ctx, &rgb_ctx.rgb, &val, &len);
	simble_srv_char_notify(&rgb_ctx.rgb, false, 8, &rgb_ctx.rgb_value);
}

void
main(void)
{
        nrf_gpio_cfg_input(TCS37717_INT_PIN, GPIO_PIN_CNF_PULL_Pullup);
        nrf_gpio_cfg_output(WLED_CTRL_PIN);

        twi_master_init();
        disable_i2c();

        simble_init("RGB/Proximity");
        proximity_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        rgb_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        //Set the timer parameters and initialize it.
        struct rtc_ctx rtc_ctx = {
                .rtc_x[NOTIF_TIMER_ID] = {
                        .type = PERIODIC,
                        .period = proximity_ctx.sampling_period,
                        .enabled = false,
                        .cb = proximity_notif_timer_cb,
                },
                .rtc_x[NOTIF_TIMER_ID+1] = {
                        .type = PERIODIC,
                        .period = rgb_ctx.sampling_period,
                        .enabled = false,
                        .cb = rgb_notif_timer_cb,
                }
        };
        batt_serv_init(&rtc_ctx);
        rtc_init(&rtc_ctx);
        ind_init();
        proximity_init(&proximity_ctx);
        rgb_init(&rgb_ctx);
        simble_adv_start();

        simble_process_event_loop();
}
