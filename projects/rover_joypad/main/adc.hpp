#pragma once

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include <memory>
#include <vector>

extern std::shared_ptr<esp_adc_cal_characteristics_t> adc1_calibration, adc2_calibration;

class AdcChannel
{
public:
	AdcChannel(adc_unit_t unit, adc_channel_t chan, adc_bits_width_t width, adc_atten_t atten);

	int get_raw();
	int get_raw_averaged(size_t num_samples);
	
	int get_voltage();
	int get_voltage_averaged(size_t num_samples);

private:
	adc_unit_t unit;
	adc_channel_t channel;
	adc_bits_width_t width;
	adc_atten_t atten;
	std::shared_ptr<esp_adc_cal_characteristics_t> calibration;
};
