#include <stdlib.h>
#include <string.h>

#include <ble.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>


struct ble_gap_advdata {
        uint8_t length;
        uint8_t data[BLE_GAP_ADV_MAX_SIZE];
};

struct ble_gap_ad_header {
        uint8_t payload_length;
        uint8_t type;
} __packed;

struct ble_gap_ad_flags {
        struct ble_gap_ad_header;
        uint8_t flags;
} __packed;

struct ble_gap_ad_name {
        struct ble_gap_ad_header;
        uint8_t name[BLE_GAP_DEVNAME_MAX_LEN];
} __packed;


/* SD upcall */
void
SWI2_IRQHandler(void)
{
        uint32_t r;

        do {
                uint32_t evt;

                r = sd_evt_get(&evt);
        } while (r == NRF_SUCCESS);

        do {
                uint8_t evt_buf[GATT_MTU_SIZE_DEFAULT + sizeof(ble_evt_t)];
                uint16_t len = sizeof(evt_buf);

                r = sd_ble_evt_get(evt_buf, &len);
        } while (r == NRF_SUCCESS);
}

static uint32_t
ble_add_advdata(const struct ble_gap_ad_header *data, struct ble_gap_advdata *advdata)
{
        size_t elem_length = data->payload_length + sizeof(*data);
        size_t final_length = advdata->length + elem_length;

        if (final_length > sizeof(advdata->data))
                return NRF_ERROR_DATA_SIZE;

        memcpy(&advdata->data[advdata->length], data, elem_length);
        advdata->data[advdata->length] = elem_length - 1;

        advdata->length += elem_length;

        return NRF_SUCCESS;
}

static void
ble_adv_start(void)
{
        struct ble_gap_advdata advdata = { .length = 0 };

        struct ble_gap_ad_flags flags = {
                .payload_length = 1,
                .type = BLE_GAP_AD_TYPE_FLAGS,
                .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        };
        ble_add_advdata(&flags, &advdata);

        struct ble_gap_ad_name name;
        uint16_t namelen = sizeof(advdata);
        if (sd_ble_gap_device_name_get(name.name, &namelen) != NRF_SUCCESS)
                namelen = 0;
        name.type =  BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
        name.payload_length = namelen;
        ble_add_advdata(&name, &advdata);

        sd_ble_gap_adv_data_set(advdata.data, advdata.length, NULL, 0);

        ble_gap_adv_params_t adv_params = {
                .type = BLE_GAP_ADV_TYPE_ADV_IND,
                .fp = BLE_GAP_ADV_FP_ANY,
                .interval = 0x400,
        };
        sd_ble_gap_adv_start(&adv_params);
}

static void
ble_init(const char *name)
{
        sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL); /* XXX assertion handler */

        ble_enable_params_t ble_params = {
                .gatts_enable_params = {
                        .service_changed = 1,
                },
        };
        sd_ble_enable(&ble_params);

        ble_gap_conn_sec_mode_t mode;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&mode);
        sd_ble_gap_device_name_set(&mode, (const uint8_t *)name, strlen(name));
}


void
main(void)
{
        ble_init("foo");
        ble_adv_start();

        for (;;)
                sd_app_evt_wait();
}
