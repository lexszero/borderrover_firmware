#pragma once

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "core_control_types.hpp"
#include "util_misc.hpp"
#include "util_task.hpp"
#include "NeoPixelBus.h"
#include "WS2812FX.h"

#include <functional>
#include <list>
#include <ostream>

namespace Leds {

using nlohmann::json;
using Core::ControlPushButton;
using Core::ControlRange;
using Core::ControlSwitch;

void to_json(json& j, const RgbColor& color);
std::ostream& operator<<(std::ostream& os, const RgbColor& color);

class ControlRGB : public Core::GenericControl<RgbColor> {
public:
	using GenericControl<RgbColor>::GenericControl;
	using GenericControl<RgbColor>::operator=;
	
	/*
	void mqttMeta() {
		AbstractControl::mqttMetaDefault("rgb");
	}
	*/

	virtual void from_string(const std::string& newval) override;
	virtual std::string to_string() override;
	virtual void append_json(json& j) override;
};

class Output;

class Segment {
public:
	Segment() = delete;
	Segment(const Segment&) = delete;

	explicit Segment(Output& output, const std::string& prefix, uint8_t _id, uint16_t start, uint16_t stop, bool reverse = false);

	Output& output;
	const uint8_t id;

	ControlRange<uint8_t, MODE_COUNT> mode;
	ControlRange<uint8_t, 255> speed;
	ControlRGB colors[3];
	ControlPushButton next, prev;
};

class Output :
	protected Task
{
public:
	Output() = delete;
	Output(const Output&) = delete;
	explicit Output(uint16_t num_leds, uint8_t pin, neoPixelType type);

	WS2812FX leds;
	std::list<Segment> segments;

	ControlSwitch artnet;
	ControlRange<uint8_t, 255> brightness;
	ControlSwitch sync;
	ControlPushButton trigger;

	void start();

	Segment& add_segment(const std::string& prefix, uint16_t start, uint16_t stop);
	void sync_segments(uint8_t seg, std::function<void(Segment&)> func);
	void set_color(uint8_t seg, uint8_t n, const RgbColor& val);

private:
	bool sync_in_progress;

protected:
	virtual void run() override;
};

} // namespace Leds
