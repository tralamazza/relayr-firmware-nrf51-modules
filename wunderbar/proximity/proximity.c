#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"

#include "tcs3771.h"


struct proximity_ctx {
        struct service_desc;
        struct char_desc proximity;
        uint16_t proximity_value;
};


static void
proximity_update(struct proximity_ctx *ctx, uint16_t val)
{
        simble_srv_char_update(&ctx->proximity, &val);
}

static void
proximity_connected(struct service_desc *s)
{
        tcs3771_init();
}

static void
proximity_disconnected(struct service_desc *s)
{
        tcs3771_stop();
}

static void
proximity_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct proximity_ctx *ctx = (void *)s;

        ctx->proximity_value = tcs3771_proximity_data();
        *lenp = sizeof(ctx->proximity_value);
        *valp = &ctx->proximity_value;
}

static void
proximity_init(struct proximity_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->proximity,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_PROXIMITY_CHAR,
                     u8"Proximity",
                     2);
        ctx->proximity.read_cb = proximity_read;
        /* srv_char_attach_format(&ctx->proximity, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->connect_cb = proximity_connected;
        ctx->disconnect_cb = proximity_disconnected;
        simble_srv_register(ctx);
}


static struct proximity_ctx proximity_ctx;

void
main(void)
{
        twi_master_init();

        simble_init("proximity");
        ind_init();
        batt_serv_init();
        proximity_init(&proximity_ctx);
        simble_adv_start();

        //proximity_update(&proximity_ctx, 1234);

        simble_process_event_loop();
}
