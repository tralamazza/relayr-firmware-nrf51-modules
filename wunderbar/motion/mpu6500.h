struct mpu6500_data {
        uint16_t accel_x;
        uint16_t accel_y;
        uint16_t accel_z;
        uint16_t gyro_x;
        uint16_t gyro_y;
        uint16_t gyro_z;
};

void mpu6500_start(void);
void mpu6500_stop(void);
void mpu6500_init(void);
void mpu6500_read_data(struct mpu6500_data *outdata);
