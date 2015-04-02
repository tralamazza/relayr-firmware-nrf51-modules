#include <stdlib.h>
#include <string.h>

#include <nrf_gpio.h>
#include <nrf_delay.h>

#include "simble.h"
#include "indicator.h"
#include "batt_serv.h"
#include "onboard-led.h"
#include "rtc.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN SAMPLING_PERIOD 250UL

#define NOTIF_TIMER_ID  0

#define CONV_WAKEUP_TIME 75000

enum noise_level_pins {
	noise_level_pin_CONVERTER = 11,
	noise_level_pin_OPAMP = 12,
	noise_level_pin_SWITCH_ON = 13,
};

struct noiselvl_ctx {
	struct service_desc;
	struct char_desc noiselvl;
	struct char_desc sampling_period_noiselvl;
	uint16_t last_reading;
	uint32_t sampling_period;
};

static struct noiselvl_ctx noiselvl_ctx;


static void
enable_converter(bool value)
{
	nrf_gpio_pin_write(noise_level_pin_CONVERTER, !value);
	nrf_gpio_pin_write(noise_level_pin_OPAMP, value);
	nrf_gpio_pin_write(noise_level_pin_SWITCH_ON, value);
}

void
ADC_IRQHandler(void)
{
	if (NRF_ADC->EVENTS_END == 0) {
		return;
	}
	NRF_ADC->EVENTS_END = 0;
	noiselvl_ctx.last_reading = NRF_ADC->RESULT;
	NRF_ADC->TASKS_STOP = 1;
	simble_srv_char_update(&noiselvl_ctx.noiselvl, &noiselvl_ctx.last_reading);
	enable_converter(false);
}

static void
adc_config()
{
	NRF_ADC->CONFIG = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
			(ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos) |
			(ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) |
			(ADC_CONFIG_PSEL_AnalogInput7 << ADC_CONFIG_PSEL_Pos) |
			(ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
}

static void
adc_read_start()
{
	NRF_ADC->EVENTS_END = 0;
	NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;
	sd_nvic_ClearPendingIRQ(ADC_IRQn);
	sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);
	sd_nvic_EnableIRQ(ADC_IRQn);
	adc_config();
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	NRF_ADC->TASKS_START = 1;
}

static uint16_t
adc_read_blocking()
{
	while (NRF_ADC->BUSY == 1) {
		// __asm("nop");
	}
	NRF_ADC->EVENTS_END = 0;
	sd_nvic_DisableIRQ(ADC_IRQn);
	NRF_ADC->INTENCLR = ADC_INTENCLR_END_Enabled;
	NRF_ADC->TASKS_START = 1;
	adc_config();
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	while (NRF_ADC->EVENTS_END == 0) {
		// __asm("nop");
	}
	uint16_t result = NRF_ADC->RESULT;
	NRF_ADC->EVENTS_END = 0;
	NRF_ADC->TASKS_STOP = 1;
	return result;
}

static void
noiselvl_connected(struct service_desc *s)
{
	enable_converter(true);
	nrf_delay_us(CONV_WAKEUP_TIME);
	adc_read_start();
}

static void
noiselvl_disconnected(struct service_desc *s)
{
	enable_converter(false);
	rtc_update_cfg(noiselvl_ctx.sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
noiselvl_read_cb(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len)
{
	struct noiselvl_ctx *ctx = (struct noiselvl_ctx *) s;
	enable_converter(true);
	nrf_delay_us(CONV_WAKEUP_TIME);
	ctx->last_reading = adc_read_blocking();
	enable_converter(false);
	*val = &ctx->last_reading;
	*len = 2;
}

static void
sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct noiselvl_ctx *ctx = (struct noiselvl_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct noiselvl_ctx *ctx = (struct noiselvl_ctx *)s;
	if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
}

void
noiselvl_notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct noiselvl_ctx *ctx = (struct noiselvl_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)NOTIF_TIMER_ID, false);
}

static void
noiselvl_init(struct noiselvl_ctx* ctx)
{
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	simble_srv_char_add(ctx, &ctx->noiselvl,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SOUND_CHAR,
		u8"Noise level",2);
	simble_srv_char_attach_format(&ctx->noiselvl,
		BLE_GATT_CPF_FORMAT_UINT16,
		0,
		ORG_BLUETOOTH_UNIT_UNITLESS);
	simble_srv_char_add(ctx, &ctx->sampling_period_noiselvl,
		simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
		u8"sampling period",
		sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_noiselvl,
		BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);
	ctx->connect_cb = noiselvl_connected;
	ctx->disconnect_cb = noiselvl_disconnected;
	ctx->noiselvl.read_cb = noiselvl_read_cb;
	ctx->noiselvl.notify = 1;
        ctx->noiselvl.notify_status_cb = noiselvl_notify_status_cb;
        ctx->sampling_period_noiselvl.read_cb = sampling_period_read_cb;
	ctx->sampling_period_noiselvl.write_cb = sampling_period_write_cb;
	simble_srv_register(ctx);
}

static void
gpio_init(void)
{
	nrf_gpio_cfg_output(noise_level_pin_CONVERTER);
	nrf_gpio_cfg_output(noise_level_pin_OPAMP);
	nrf_gpio_cfg_output(noise_level_pin_SWITCH_ON);
}

static void
notif_timer_cb(struct rtc_ctx *ctx)
{
	void *val = &noiselvl_ctx.last_reading;
	uint16_t len = sizeof(noiselvl_ctx.last_reading);
	noiselvl_read_cb(&noiselvl_ctx, &noiselvl_ctx.noiselvl, &val, &len);
        simble_srv_char_notify(&noiselvl_ctx.noiselvl, false, len, val);
}

void
main(void)
{
	gpio_init();
	enable_converter(false);

	simble_init("Noise level");
	noiselvl_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        //Set the timer parameters and initialize it.
        struct rtc_ctx rtc_ctx = {
                .rtc_x[NOTIF_TIMER_ID] = {
                        .type = PERIODIC,
                        .period = noiselvl_ctx.sampling_period,
                        .enabled = false,
                        .cb = notif_timer_cb,
                }
        };
	batt_serv_init(&rtc_ctx);
        rtc_init(&rtc_ctx);
	ind_init();
	noiselvl_init(&noiselvl_ctx);
	simble_adv_start();
	simble_process_event_loop();
}
