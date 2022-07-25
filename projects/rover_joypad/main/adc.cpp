#include "adc.hpp"

static constexpr uint32_t DEFAULT_VREF = 1100;

std::shared_ptr<esp_adc_cal_characteristics_t> adc1_calibration, adc2_calibration;

AdcChannel::AdcChannel(adc_unit_t unit, adc_channel_t chan, adc_bits_width_t width, adc_atten_t atten) :
	unit(unit),
	channel(chan),
	width(width),
	atten(atten)
{
	if (unit == ADC_UNIT_1) {
		adc1_config_width(width);
		adc1_config_channel_atten(static_cast<adc1_channel_t>(chan), atten);
		if (!adc1_calibration) {
			adc1_calibration = std::make_shared<esp_adc_cal_characteristics_t>();
			esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc1_calibration.get());
		}
		calibration = adc1_calibration;
	}
	else {
		adc2_config_channel_atten(static_cast<adc2_channel_t>(chan), atten);
		if (!adc2_calibration) {
			adc2_calibration = std::make_shared<esp_adc_cal_characteristics_t>();
			esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc2_calibration.get());
		}
		calibration = adc2_calibration;
	}
}

int AdcChannel::get_raw()
{
	if (unit == ADC_UNIT_1) {
		return adc1_get_raw(static_cast<adc1_channel_t>(channel));
	}
	else {
		int raw;
		adc2_get_raw(static_cast<adc2_channel_t>(channel), width, &raw);
		return static_cast<uint32_t>(raw);
	}
}

int AdcChannel::get_voltage()
{
	return esp_adc_cal_raw_to_voltage(get_raw(), calibration.get());
}

int AdcChannel::get_raw_averaged(size_t num_samples)
{
	uint32_t reading = 0;
	for (int i = 0; i < num_samples; i++) {
		reading += get_raw();
	}
	return reading / num_samples;
}

int AdcChannel::get_voltage_averaged(size_t num_samples)
{
	auto raw = get_raw_averaged(num_samples);
	return esp_adc_cal_raw_to_voltage(raw, calibration.get());
}
