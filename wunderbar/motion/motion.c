#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"

#include "mpu6500.h"


struct motion_ctx {
        struct service_desc;
        struct char_desc motion;
        struct mpu6500_data motion_value;
};


static void
motion_update(struct motion_ctx *ctx, struct mpu6500_data *val)
{
        simble_srv_char_update(&ctx->motion, val);
}

static void
motion_connected(struct service_desc *s)
{
        mpu6500_start();
}

static void
motion_disconnected(struct service_desc *s)
{
        mpu6500_stop();
}

static void
motion_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct motion_ctx *ctx = (void *)s;

        mpu6500_read_data(&ctx->motion_value);
        *lenp = sizeof(ctx->motion_value);
        *valp = &ctx->motion_value;
}

static void
motion_init(struct motion_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->motion,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_MOTION_CHAR,
                     u8"Motion",
                     sizeof(ctx->motion_value));
        ctx->motion.read_cb = motion_read;
        /* srv_char_attach_format(&ctx->motion, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->connect_cb = motion_connected;
        ctx->disconnect_cb = motion_disconnected;
        simble_srv_register(ctx);
}


static struct motion_ctx motion_ctx;

void
main(void)
{
        twi_master_init();
        mpu6500_init();
        mpu6500_stop();

        simble_init("motion");
        ind_init();
        batt_serv_init();
        motion_init(&motion_ctx);
        simble_adv_start();

        simble_process_event_loop();
}
