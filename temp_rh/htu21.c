#include "htu21.h"
#include "twi_master.h"
#include "crc8.h"

#define ROUNDED_DIV(A, B) (((A) + ((B) / 2)) / (B))

struct htu21_bock_reading_t {
	union {
		struct {
			uint16_t measurement : 14;
			uint16_t is_humidity : 1;
			uint16_t pad : 1;
			uint8_t checksum;
		};
		uint8_t raw[3];
	};
};

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
	struct htu21_bock_reading_t result;
	bool ret = twi_master_transfer(HTU21_ADDRESS_W, &cmd, 1, TWI_DONT_ISSUE_STOP) &&
		twi_master_transfer(HTU21_ADDRESS_R, (uint8_t*)&result, 3, TWI_ISSUE_STOP);
	// TODO either use checksum or don't read it (read 2 bytes and remove the field)
	if (ret)
		*reading = result.measurement;
	return ret;
}

void htu21_reset()
{
	enum htu21_command_t cmd = HTU21_SOFT_RESET;
	twi_master_transfer(HTU21_ADDRESS_W, &cmd, 1, TWI_ISSUE_STOP);
}

bool htu21_read_temperature(int8_t *value)
{
	uint16_t reading = 0;
	if (!htu21_block_reading(HTU21_READ_TEMPERATURE_BLOCKING, &reading))
		return false;
	*value = ROUNDED_DIV( ((21965 * reading) >> 13) - 46850, 1000 );
	// = -46.85 + 175.72 * (reading / (1 << 16));
	return true;
}

bool htu21_read_humidity(uint8_t *value)
{
	uint16_t reading = 0;
	if (!htu21_block_reading(HTU21_READ_HUMIDITY_BLOCKING, &reading))
		return false;
	*value = ROUNDED_DIV( ((15625 * reading) >> 13) - 6000, 1000 );
	// = -6 + 125 * (reading / (1 << 16));
	return true;
}

bool htu21_read_user_register(struct htu21_user_register_t* user_reg)
{
	enum htu21_command_t cmd = HTU21_READ_USER_REG;
	twi_master_transfer(HTU21_ADDRESS_W, &cmd, 1, TWI_DONT_ISSUE_STOP);
	bool ret = twi_master_transfer(HTU21_ADDRESS_R, &user_reg->raw, 1, TWI_ISSUE_STOP);
	user_reg->raw = rotl(user_reg->raw, 1);
	return ret;
}

bool htu21_write_user_register(struct htu21_user_register_t* user_reg)
{
	uint8_t cmd[2] = { HTU21_WRITE_USER_REG, rotr(user_reg->raw, 1) };
	return twi_master_transfer(HTU21_ADDRESS_W, cmd, 2, TWI_ISSUE_STOP);
}