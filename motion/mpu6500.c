#include <sys/types.h>
#include <stdint.h>
#include <string.h>

#include <twi_master.h>

#include "util.h"
#include "mpu6500.h"


#define MPU6500 0xd0

enum mpu6500_reg_addr {
        MPU6500_INT_STATUS = 58,
        MPU6500_ACCEL_XOUT = 59,
        MPU6500_PWR_MGMT_1 = 107,
        MPU6500_PWR_MGMT_2 = 108,
};

static void
mpu6500_write_register(enum mpu6500_reg_addr addr, uint8_t *data, size_t len)
{
        uint8_t pkt[len + 1];
        pkt[0] = addr;
        memcpy(&pkt[1], data, len);
        twi_master_transfer(MPU6500, pkt, sizeof(pkt), TWI_ISSUE_STOP);
}

static void
mpu6500_read_register(enum mpu6500_reg_addr addr, uint8_t *data, size_t len)
{
        twi_master_transfer(MPU6500, &addr, sizeof(addr), TWI_DONT_ISSUE_STOP);
        twi_master_transfer(MPU6500 | TWI_READ_BIT, data, len, TWI_ISSUE_STOP);
}

void
mpu6500_start(void)
{
        uint8_t val[] = {0x01};
        mpu6500_write_register(MPU6500_PWR_MGMT_1, val, sizeof(val));
}

void
mpu6500_stop(void)
{
        uint8_t val[] = {0x41};
        mpu6500_write_register(MPU6500_PWR_MGMT_1, val, sizeof(val));
}

void
mpu6500_init(void)
{
        uint8_t val[] = {0x80};
        mpu6500_write_register(MPU6500_PWR_MGMT_1, val, sizeof(val));

        while (val[0] & 0x80) {
                mpu6500_read_register(MPU6500_PWR_MGMT_1, val, sizeof(val));
        }
}

void
mpu6500_read_data(struct mpu6500_data *outdata)
{
        struct {
                uint16_t accel_x, accel_y, accel_z;
                uint16_t temp;
                uint16_t gyro_x, gyro_y, gyro_z;
        } data;
        mpu6500_read_register(MPU6500_ACCEL_XOUT, (void *)&data, sizeof(data));
        outdata->accel_x = be16toh(data.accel_x);
        outdata->accel_y = be16toh(data.accel_y);
        outdata->accel_z = be16toh(data.accel_z);
        outdata->gyro_x = be16toh(data.gyro_x);
        outdata->gyro_y = be16toh(data.gyro_y);
        outdata->gyro_z = be16toh(data.gyro_z);
}

/* uint8_t */
/* mpu6500_status(void) */
/* { */
/*         uint8_t val; */
/*         mpu6500_read_register(MPU6500_INT_STATUS, &val, sizeof(val)); */
/*         return (val); */
/* } */
