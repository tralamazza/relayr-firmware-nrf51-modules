#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"


struct proximity_ctx {
        struct service_desc;
        struct char_desc proximity;
};


static void
proximity_update(struct proximity_ctx *ctx, uint16_t val)
{
        srv_char_update(&ctx->proximity, &val);
}

static void
proximity_connected(struct service_desc *s)
{
        struct proximity_ctx *ctx = (void *)s;
        uint8_t cmd = 0xa0 | 0x12;
        uint16_t val;

        twi_master_transfer((0x29 << 1), &cmd, 1, TWI_DONT_ISSUE_STOP);
        twi_master_transfer((0x29 << 1) | TWI_READ_BIT, (void *)&val, 2, TWI_ISSUE_STOP);

        proximity_update(ctx, val);
}

static void
proximity_disconnected(struct service_desc *s)
{
        struct proximity_ctx *ctx = (void *)s;

        /* XXX switch off sensor/twi */
}

static void
proximity_init(struct proximity_ctx *ctx)
{
        srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        srv_char_add(ctx, &ctx->proximity,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_PROXIMITY_CHAR,
                     u8"Proximity",
                     2);
        /* srv_char_attach_format(&ctx->proximity, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->connect_cb = proximity_connected;
        ctx->disconnect_cb = proximity_disconnected;
        srv_register(ctx);
}


static struct proximity_ctx proximity_ctx;

void
main(void)
{
        twi_master_init();

        simble_init("proximity");
        ind_init();
        proximity_init(&proximity_ctx);
        simble_adv_start();

        //proximity_update(&proximity_ctx, 1234);

        simble_process_event_loop();
}
