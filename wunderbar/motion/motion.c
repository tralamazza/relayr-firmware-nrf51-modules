#include <stdlib.h>
#include <string.h>

#include <twi_master.h>
#include <nrf_delay.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"
#include "i2c.h"
#include "rtc.h"

#include "mpu6500.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN_SAMPLING_PERIOD 250UL

#define NOTIF_TIMER_ID  0


struct motion_ctx {
        struct service_desc;
        struct char_desc motion;
        struct char_desc sampling_period_motion;
        struct mpu6500_data motion_value;
        uint32_t sampling_period;
};

static struct motion_ctx motion_ctx;


static void
motion_update(struct motion_ctx *ctx, struct mpu6500_data *val)
{
        simble_srv_char_update(&ctx->motion, val);
}

static void
motion_disconnected(struct service_desc *s)
{
        rtc_update_cfg(motion_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
motion_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct motion_ctx *ctx = (void *)s;

        enable_i2c();
        mpu6500_start();
        nrf_delay_us(MPU6500_WAKEUP_TIME);
        mpu6500_read_data(&ctx->motion_value);
        mpu6500_stop();
        disable_i2c();
        *lenp = sizeof(ctx->motion_value);
        *valp = &ctx->motion_value;
}

static void
sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct motion_ctx *ctx = (struct motion_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct motion_ctx *ctx = (struct motion_ctx *)s;

        if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
motion_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct motion_ctx *ctx = (struct motion_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
motion_init(struct motion_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->motion,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_MOTION_CHAR,
                     u8"Motion",
                     sizeof(ctx->motion_value));
        simble_srv_char_add(ctx, &ctx->sampling_period_motion,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
                     u8"sampling period",
                     sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_motion,
	             BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
        /* srv_char_attach_format(&ctx->motion, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->disconnect_cb = motion_disconnected;
        ctx->motion.read_cb = motion_read;
        ctx->motion.notify = 1;
        ctx->motion.notify_status_cb = motion_notify_status_cb;
        ctx->sampling_period_motion.read_cb = sampling_period_read_cb;
	ctx->sampling_period_motion.write_cb = sampling_period_write_cb;
        simble_srv_register(ctx);
}

static void
notif_timer_cb(struct rtc_ctx *ctx)
{
        void *val = &motion_ctx.motion_value;
	uint16_t len = sizeof(motion_ctx.motion_value);
        motion_read(&motion_ctx, &motion_ctx.motion, &val, &len);
        simble_srv_char_notify(&motion_ctx.motion, false, len,
                &motion_ctx.motion_value);
}

void
main(void)
{
        twi_master_init();
        mpu6500_init();
        mpu6500_stop();
        disable_i2c();

        simble_init("Motion");

        motion_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        //Set the timer parameters and initialize it.
        struct rtc_ctx rtc_ctx = {
                .rtc_x[NOTIF_TIMER_ID] = {
                        .type = PERIODIC,
                        .period = motion_ctx.sampling_period,
                        .enabled = false,
                        .cb = notif_timer_cb,
                }
        };
        batt_serv_init(&rtc_ctx);
        rtc_init(&rtc_ctx);

        ind_init();
        motion_init(&motion_ctx);
        simble_adv_start();

        simble_process_event_loop();
}
