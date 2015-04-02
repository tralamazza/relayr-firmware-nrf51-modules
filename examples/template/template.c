#include "simble.h"
#include "onboard-led.h"
#include "rtc.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN SAMPLING_PERIOD 250UL

#define NOTIF_TIMER_ID  0

struct my_service_ctx {
        struct service_desc;
        struct char_desc my_char_data;
        struct char_desc sampling_period_char;
        uint8_t my_sensor_value;
        uint32_t sampling_period;
};

static void
template_connect_cb(struct service_desc *s)
{
	// struct my_service_ctx *ctx = s;
}

static void
template_disconnect_cb(struct service_desc *s)
{
	// struct my_service_ctx *ctx = s;
}

static void
my_char_data_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	*valp = &ctx->my_sensor_value;
	*lenp = 1;

        //example for the 1-shot timer:
        rtc_evt_cb_t my_one_shot_timer_cb;
        if (rtc_oneshot_timer(100, my_one_shot_timer_cb)){
                onboard_led(ONBOARD_LED_ON);
        }
}

void
my_one_shot_timer_cb(struct rtc_ctx *ctx)
{
        onboard_led(ONBOARD_LED_OFF);
}

static void
my_char_data_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	ctx->my_sensor_value = *(uint8_t*)val;
}

static void
sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	ctx->sampling_period = *(uint32_t*)val;

        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
mychar_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct my_service_ctx *ctx = (struct my_service_ctx *)s;

        if ((status & BLE_GATT_HVX_NOTIFICATION) && (ctx->sampling_period > MIN_SAMPLING_PERIOD))
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}


static void
my_service_init(struct my_service_ctx *ctx)
{
	ctx->my_sensor_value = 0;
	// init the service context
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	// add a characteristic to our service
	simble_srv_char_add(ctx, &ctx->my_char_data,
		simble_get_vendor_uuid_class(), VENDOR_UUID_RAW_CHAR,
		u8"my characteristic",
		1); // size in bytes

        simble_srv_char_add(ctx, &ctx->sampling_period_char,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_char,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
	// BLE callbacks (optional)
	ctx->connect_cb = template_connect_cb;
	ctx->disconnect_cb = template_disconnect_cb;
	ctx->my_char_data.read_cb = my_char_data_read_cb;
	ctx->my_char_data.write_cb = my_char_data_write_cb;
        ctx->my_char_data.notify = 1;
        ctx->my_char_data.notify_status_cb = mychar_notify_status_cb;

        ctx->sampling_period_char.read_cb = sampling_period_read_cb;
	ctx->sampling_period_char.write_cb = sampling_period_write_cb;
	simble_srv_register(ctx); // register our service
}

static struct my_service_ctx my_service_ctx;


static void
notif_timer_cb(struct rtc_ctx *ctx)
{
        my_service_ctx.my_sensor_value++;
        simble_srv_char_notify(&my_service_ctx.my_char_data, false, 1,
                &my_service_ctx.my_sensor_value);
}

void
main(void)
{
	simble_init("relayr_template"); // init BLE library

        my_service_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        //Set the timer parameters and initialize it.
        struct rtc_ctx rtc_ctx = {
                .rtc_x[NOTIF_TIMER_ID] = {
                        .type = PERIODIC,
                        .period = my_service_ctx.sampling_period,
                        .enabled = false,
                        .cb = notif_timer_cb,
                }
        };

        // NOTE: rtc_init needs to be called AFTER simble_init which configures
        //       the LFCLKSRC
        rtc_init(&rtc_ctx);

	my_service_init(&my_service_ctx);
	simble_adv_start(); // start advertising
	simble_process_event_loop(); // main loop (stuck here)
}
