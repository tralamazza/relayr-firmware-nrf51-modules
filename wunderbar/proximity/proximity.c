#include <stdlib.h>
#include <string.h>

#include <twi_master.h>
#include <nrf_gpio.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"

#include "tcs3771.h"

#define WLED_CTRL_PIN 21

struct proximity_ctx {
        struct service_desc;
        struct char_desc proximity;
        uint16_t proximity_value;
};

struct rgb_ctx {
        struct service_desc;
        struct char_desc rgb;
        uint64_t rgb_value;
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


static void
rgb_update(struct rgb_ctx *ctx, uint8_t *val)
{
        simble_srv_char_update(&ctx->rgb, val);
}

static void
rgb_connected(struct service_desc *s)
{
        tcs3771_init();
}

static void
rgb_disconnected(struct service_desc *s)
{
        tcs3771_stop();
}

static void
rgb_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct rgb_ctx *ctx = (void *)s;

        nrf_gpio_pin_write(WLED_CTRL_PIN, true);
        ctx->rgb_value = tcs3771_rgb_data();
        nrf_gpio_pin_write(WLED_CTRL_PIN, false);
        *lenp = sizeof(ctx->rgb_value);
        *valp = &ctx->rgb_value;
}

static void
rgb_init(struct rgb_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE_2);
        simble_srv_char_add(ctx, &ctx->rgb,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_COLOR_CHAR,
                     u8"RGB",
                     8);
        ctx->rgb.read_cb = rgb_read;
        /* srv_char_attach_format(&ctx->proximity, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->connect_cb = rgb_connected;
        ctx->disconnect_cb = rgb_disconnected;
        simble_srv_register(ctx);
}


static struct proximity_ctx proximity_ctx;
static struct rgb_ctx rgb_ctx;

void
main(void)
{
        nrf_gpio_cfg_output(WLED_CTRL_PIN);

        twi_master_init();

        simble_init("RGB/Proximity");
        ind_init();
        batt_serv_init();
        proximity_init(&proximity_ctx);
        rgb_init(&rgb_ctx);
        simble_adv_start();
        simble_process_event_loop();
}
