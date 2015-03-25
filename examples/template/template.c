#include "simble.h"
#include "onboard-led.h"
#include "rtc.h"

# define DEFAULT_SAMPLING_RATE 1000

struct my_service_ctx {
        struct service_desc;
        struct char_desc my_char_data;
        struct char_desc sampling_rate_char;
        uint8_t my_sensor_value;
        uint32_t sampling_rate;
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
}

static void
my_char_data_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	ctx->my_sensor_value = *(uint8_t*)val;
}

static void
sampling_rate_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	*valp = &ctx->sampling_rate;
	*lenp = sizeof(&ctx->sampling_rate);
}

static void
sampling_rate_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	ctx->sampling_rate = *(uint32_t*)val;
  #pragma message "TODO: implement period change in rtc module"
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

  simble_srv_char_add(ctx, &ctx->sampling_rate_char,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_RATE_CHAR,
		u8"sampling rate",
		sizeof(&ctx->sampling_rate)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
  simble_srv_char_attach_format(&ctx->sampling_rate_char,
		BLE_GATT_CPF_FORMAT_UINT24,
		0,
    ORG_BLUETOOTH_UNIT_UNITLESS);

	// BLE callbacks (optional)
	ctx->connect_cb = template_connect_cb;
	ctx->disconnect_cb = template_disconnect_cb;
	ctx->my_char_data.read_cb = my_char_data_read_cb;
	ctx->my_char_data.write_cb = my_char_data_write_cb;
  ctx->my_char_data.notify = 1;

  ctx->sampling_rate_char.read_cb = sampling_rate_read_cb;
	ctx->sampling_rate_char.write_cb = sampling_rate_write_cb;
	simble_srv_register(ctx); // register our service
}

static struct my_service_ctx my_service_ctx;


static void
notif_timer_cb(struct rtc_ctx *ctx)
{
  NRF_GPIO->OUT ^= (1 << 1);
  my_service_ctx.my_sensor_value++;
  simble_srv_char_notify(&my_service_ctx.my_char_data, false, 1, &my_service_ctx.my_sensor_value);
}

void
main(void)
{
	simble_init("relayr"); // init BLE library

  NRF_GPIO->PIN_CNF[2] = GPIO_PIN_CNF_DIR_Output;
  NRF_GPIO->PIN_CNF[1] = GPIO_PIN_CNF_DIR_Output;

  my_service_ctx.sampling_rate = DEFAULT_SAMPLING_RATE;
  //Set the timer parameters and initialize it.
  struct rtc_ctx rtc_ctx = {
      .rtc_x[0].period = my_service_ctx.sampling_rate,
      .rtc_x[0].enabled = 1,
      .rtc_x[0].cb = notif_timer_cb,
  };
  // NOTE: rtc_init needs to be called AFTER simble_init which configures
  //      the LFCLKSRC
  rtc_init(&rtc_ctx);

	my_service_init(&my_service_ctx);
	simble_adv_start(); // start advertising
	simble_process_event_loop(); // main loop (stuck here)
}
