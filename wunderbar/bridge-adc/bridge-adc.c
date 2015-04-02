#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "rtc.h"

#include "adc121c02.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN_SAMPLING_PERIOD 100UL

#define NOTIF_TIMER_ID  0


struct bridge_adc_ctx {
        struct service_desc;
        struct char_desc bridge_adc;
        struct char_desc sampling_period_bridge_adc;
        uint16_t bridge_adc_value;
        uint32_t sampling_period;
};

static struct bridge_adc_ctx bridge_adc_ctx;


static void
bridge_adc_update(struct bridge_adc_ctx *ctx, uint16_t val)
{
        simble_srv_char_update(&ctx->bridge_adc, &val);
}

static void
bridge_adc_connected(struct service_desc *s)
{
        adc121c02_init();
}

static void
bridge_adc_disconnected(struct service_desc *s)
{
        adc121c02_stop();
        rtc_update_cfg(bridge_adc_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
bridge_adc_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct bridge_adc_ctx *ctx = (void *)s;

        ctx->bridge_adc_value = adc121c02_sample();
        *lenp = sizeof(ctx->bridge_adc_value);
        *valp = &ctx->bridge_adc_value;
}

static void
sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct bridge_adc_ctx *ctx = (struct bridge_adc_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct bridge_adc_ctx *ctx = (struct bridge_adc_ctx *)s;
	ctx->sampling_period = *(uint32_t*)val;

        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
bridge_adc_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct bridge_adc_ctx *ctx = (struct bridge_adc_ctx *)s;

        if ((status & BLE_GATT_HVX_NOTIFICATION) && (ctx->sampling_period > MIN_SAMPLING_PERIOD))
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
bridge_adc_init(struct bridge_adc_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->bridge_adc,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_ADC_CHAR,
                     u8"Bridge-Adc",
                     2);
        /* srv_char_attach_format(&ctx->bridge_adc, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        simble_srv_char_add(ctx, &ctx->sampling_period_bridge_adc,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_bridge_adc,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
        ctx->connect_cb = bridge_adc_connected;
        ctx->disconnect_cb = bridge_adc_disconnected;
        ctx->bridge_adc.read_cb = bridge_adc_read;
        ctx->bridge_adc.notify = 1;
        ctx->bridge_adc.notify_status_cb = bridge_adc_notify_status_cb;
        ctx->sampling_period_bridge_adc.read_cb = sampling_period_read_cb;
        ctx->sampling_period_bridge_adc.write_cb = sampling_period_write_cb;
        simble_srv_register(ctx);
}

static void
notif_timer_cb(struct rtc_ctx *ctx)
{
        bridge_adc_ctx.bridge_adc_value = adc121c02_sample();
        simble_srv_char_notify(&bridge_adc_ctx.bridge_adc, false,
                sizeof(bridge_adc_ctx.bridge_adc_value),
                &bridge_adc_ctx.bridge_adc_value);
}

void
main(void)
{
        twi_master_init();

        simble_init("Bridge-ADC");
        bridge_adc_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        //Set the timer parameters and initialize it.
        struct rtc_ctx rtc_ctx = {
                .rtc_x[NOTIF_TIMER_ID] = {
                        .type = PERIODIC,
                        .period = bridge_adc_ctx.sampling_period,
                        .enabled = false,
                        .cb = notif_timer_cb,
                }
        };
        rtc_init(&rtc_ctx);
        ind_init();
        bridge_adc_init(&bridge_adc_ctx);
        simble_adv_start();

        simble_process_event_loop();
}
