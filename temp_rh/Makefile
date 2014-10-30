PROG= temperature_humidity
SRCS= temp_rh.c crc8.c htu21.c $(wildcard ../base/*.c)

SDKSRCS+= twi_master/twi_hw_master.c
CFLAGS+= -I. -I../base/

include ../nrf51-sdk/build.mk
