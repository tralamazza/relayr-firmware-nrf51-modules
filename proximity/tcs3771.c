#include <sys/types.h>
#include <string.h>

#include <twi_master.h>

#define TCS3771 (0x29 << 1)

#define TCS3771_COMMAND_TYPE_AUTOINC (0b01 << 5)
#define TCS3771_COMMAND_SELECT (1 << 7)

#define TCS3771_ENABLE 0x00
#define TCS3771_ENABLE_PON (1 << 0)
#define TCS3771_ENABLE_AEN (1 << 1)
#define TCS3771_ENABLE_PEN (1 << 2)
#define TCS3771_ENABLE_WEN (1 << 3)
#define TCS3771_ENABLE_AIEN (1 << 4)
#define TCS3771_ENABLE_PIEN (1 << 5)

#define TCS3771_ATIME 0x01
#define TCS3771_PTIME 0x02
#define TCS3771_WTIME 0x03
#define TCS3771_AILT 0x04
#define TCS3771_AIHT 0x06
#define TCS3771_PILT 0x08
#define TCS3771_PIHT 0x0a

#define TCS3771_PERS 0x0c
#define TCS3771_PERS_PPERS(x) ((x) << 4)
#define TCS3771_PERS_APERS(x) ((x) & 0xf)

#define TCS3771_CONF 0x0d
#define TCS3771_CONF_WLONG (1 << 1)

#define TCS3771_PPULSE 0x0e

#define TCS3771_CONTROL 0x0f
#define TCS3771_CONTROL_PDIODE_IR (0b10 << 4)

#define TCS3771_ID 0x12
#define TCS3771_STATUS 0x13
#define TCS3771_PDATA 0x1c

static void
tcs3771_select_register(uint8_t addr)
{
        uint8_t cmd[] = {
                TCS3771_COMMAND_SELECT | TCS3771_COMMAND_TYPE_AUTOINC | addr,
        };

        twi_master_transfer(TCS3771, cmd, sizeof(cmd), TWI_DONT_ISSUE_STOP);
}

static void
tcs3771_write_register(uint8_t addr, void *data, size_t len)
{
        uint8_t packet[len + 1];
        packet[0] = TCS3771_COMMAND_SELECT | TCS3771_COMMAND_TYPE_AUTOINC | addr;
        memcpy(&packet[1], data, len);

        twi_master_transfer(TCS3771, packet, sizeof(packet), TWI_ISSUE_STOP);
}

static void
tcs3771_read_register(uint8_t addr, void *data, size_t len)
{
        tcs3771_select_register(addr);
        twi_master_transfer(TCS3771 | TWI_READ_BIT, data, len, TWI_ISSUE_STOP);
}

void
tcs3771_init(void)
{
        uint8_t data[] = {
                TCS3771_CONTROL_PDIODE_IR
        };
        tcs3771_write_register(TCS3771_CONTROL, data, sizeof(data));

        uint8_t data2[] = {
                1
        };
        tcs3771_write_register(TCS3771_PPULSE, data2, sizeof(data2));

        uint8_t data1[] = {
                TCS3771_ENABLE_PON | TCS3771_ENABLE_PEN | TCS3771_ENABLE_WEN
        };
        tcs3771_write_register(TCS3771_ENABLE, data1, sizeof(data1));
}

uint16_t
tcs3771_proximity_data(void)
{
        uint16_t val;

        tcs3771_read_register(TCS3771_PDATA, &val, sizeof(val));
        return (val);
}

uint8_t
tcs3771_status(void)
{
        uint8_t data;

        tcs3771_read_register(TCS3771_STATUS, &data, sizeof(data));
        return (data);
}
