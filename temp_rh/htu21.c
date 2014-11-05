#include <twi_master.h>

#include "htu21.h"
#include "util.h"

static inline uint8_t rotl(uint8_t value, uint8_t shift)
{
	return (value << shift) | (value >> (sizeof(value) * 8 - shift));
}

static inline uint8_t rotr(uint8_t value, uint8_t shift)
{
	return (value >> shift) | (value << (sizeof(value) * 8 - shift));
}

static bool htu21_block_reading(enum htu21_command_t cmd, uint16_t *reading)
{
	uint8_t result[3];
	if (!(twi_master_transfer(HTU21_ADDRESS, &cmd, 1, TWI_DONT_ISSUE_STOP) &&
		twi_master_transfer(HTU21_ADDRESS | TWI_READ_BIT, result, 3, TWI_ISSUE_STOP))) {
		return false;
	}
	// checksum src: HTU21D Humidity Sensor Library, SparkFun Electronics
	uint16_t raw = (result[0] << 8) | result[1];
	uint32_t remainder = raw << 8 | result[2];
	uint32_t divsor = 0x988000;
	for (int i = 0; i < 16; i++) {
		if (remainder & (uint32_t) 1 << (23 - i)) {
			remainder ^= divsor;
		}
		divsor >>= 1;
	}
	if (remainder == 0) {
		*reading = raw & 0xfffc;
		return true;
	} else {
		return false;
	}
}

void htu21_reset()
{
	enum htu21_command_t cmd = HTU21_SOFT_RESET;
	twi_master_transfer(HTU21_ADDRESS, &cmd, 1, TWI_ISSUE_STOP);
}

bool htu21_read_temperature(int8_t *value)
{
	uint16_t reading = 0;
	if (!htu21_block_reading(HTU21_READ_TEMPERATURE_BLOCKING, &reading)) {
		return false;
	}
	*value = ROUNDED_DIV( ((21965 * reading) >> 13) - 46850, 1000 );
	// = -46.85 + 175.72 * (reading / (1 << 16));
	return true;
}

bool htu21_read_humidity(uint8_t *value)
{
	uint16_t reading = 0;
	if (!htu21_block_reading(HTU21_READ_HUMIDITY_BLOCKING, &reading)) {
		return false;
	}
	*value = ROUNDED_DIV( ((15625 * reading) >> 13) - 6000, 1000 );
	// = -6 + 125 * (reading / (1 << 16));
	return true;
}

bool htu21_read_user_register(struct htu21_user_register_t* user_reg)
{
	enum htu21_command_t cmd = HTU21_READ_USER_REG;
	bool ret = twi_master_transfer(HTU21_ADDRESS, &cmd, 1, TWI_DONT_ISSUE_STOP) &&
		twi_master_transfer(HTU21_ADDRESS | TWI_READ_BIT, &user_reg->raw, 1, TWI_ISSUE_STOP);
	if (ret) {
		user_reg->raw = rotl(user_reg->raw, 1);
	}
	return ret;
}

bool htu21_write_user_register(struct htu21_user_register_t* user_reg)
{
	uint8_t cmd[2] = { HTU21_WRITE_USER_REG, rotr(user_reg->raw, 1) };
	return twi_master_transfer(HTU21_ADDRESS, cmd, 2, TWI_ISSUE_STOP);
}