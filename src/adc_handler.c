//	simble_srv_char_update(&batt_serv_ctx.batt_lvl, &batt_serv_ctx.last_reading);

#define BATTERY 			0
#define NOISE_SENSOR	1

void
ADC_IRQHandler(void)
{
	if (NRF_ADC->EVENTS_END == 0) {
		return;
	}

	NRF_ADC->EVENTS_END     = 0;
	uint16_t result = NRF_ADC->RESULT;
	NRF_ADC->TASKS_STOP     = 1;

	switch(module_id) {
		case BATTERY:
			batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(batt_serv_ctx.last_reading);
			batt_serv_ctx.last_reading = battery_level_in_percent(batt_lvl_in_milli_volts);
			simble_srv_char_update(&batt_serv_ctx.batt_lvl, &batt_serv_ctx.last_reading);
		default:
			return;
	}
}
