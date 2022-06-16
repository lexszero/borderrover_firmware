#pragma once

#include "core_control_types.hpp"
#include "util_misc.hpp"
#include "NeoPixelBus.h"
#include "WS2812FX.h"

#include <functional>

namespace Leds {

using nlohmann::json;
using Core::ControlPushButton;
using Core::ControlRange;
using Core::ControlSwitch;

class ControlRGB : public Core::GenericControl<RgbColor> {
public:
	using GenericControl<RgbColor>::GenericControl;
	using GenericControl<RgbColor>::operator=;
	
	/*
	void mqttMeta() {
		AbstractControl::mqttMetaDefault("rgb");
	}
	*/

	void from_string(const std::string& newval) override;
	std::string to_string() override;
	void append_json(json& j) override;
};

class Output;

class Segment {
public:
	Segment(Output& output, const std::string&& prefix, uint8_t _id);

	Output& output;
	const uint8_t id;

	ControlRange<uint8_t, MODE_COUNT> mode;
	ControlRange<uint8_t, 255> speed;
	ControlRGB colors[3];
	ControlPushButton next, prev;
};

class Output {
public:
	Output(uint16_t num_leds, uint8_t pin, neoPixelType type);

	WS2812FX leds;
	std::vector<Segment> segments;

	ControlSwitch artnet;
	ControlRange<uint8_t, 255> brightness;
	ControlSwitch sync;
	ControlPushButton trigger;

	void sync_segments(uint8_t seg, std::function<void(Segment&)> func);
	void set_color(uint8_t seg, uint8_t n, const RgbColor& val);

private:
	bool sync_in_progress;
};

} // namespace Leds

namespace Core {

template<>
void GenericControl<RgbColor>::append_json(json& json);

} // namespace Core
