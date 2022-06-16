#include "leds.hpp"

#include "esp_log.h"

namespace Leds {

static const char *TAG = "leds";

using namespace std::placeholders;

namespace {

uint16_t convert_speed(uint8_t mcl_speed) {
	//long ws2812_speed = mcl_speed * 256;
	uint16_t ws2812_speed = 61760 * (exp(0.0002336 * mcl_speed) - exp(-0.03181 * mcl_speed));
	ws2812_speed = SPEED_MAX - ws2812_speed;
	if (ws2812_speed < SPEED_MIN) {
		ws2812_speed = SPEED_MIN;
	}
	if (ws2812_speed > SPEED_MAX) {
		ws2812_speed = SPEED_MAX;
	}
	return ws2812_speed;
}

inline uint32_t convert_color(const RgbColor& color) {
	return
		(static_cast<uint32_t>(color.R) << 16) |
		(static_cast<uint32_t>(color.G) << 8) |
		(static_cast<uint32_t>(color.B) << 0);
}

}  // namespace

void ControlRGB::from_string(const std::string& newval)
{
	RgbColor c;
	auto pos1 = newval.find(';');
	if (pos1 == std::string::npos)
		throw std::invalid_argument("Bad RGB value format");
	auto pos2 = newval.find(';', pos1);
	if (pos2 == std::string::npos)
		throw std::invalid_argument("Bad RGB value format");
	c.R = std::stoi(newval.substr(0, pos1));
	c.G = std::stoi(newval.substr(pos1+1, pos2));
	c.B = std::stoi(newval.substr(pos2+1));
	GenericControl<RgbColor>::set(c);
}

std::string ControlRGB::to_string()
{
	auto v = this->value;
	using ::to_string;
	return to_string(v.R) + ";" + to_string(v.G) + ";" + to_string(v.B);
}

Segment::Segment(Output& _output, const std::string&& prefix, uint8_t _id) :
	output(_output),
	id(_id),
	mode(prefix + "_mode", "Animation mode", (id+1)*10+1,
			FX_MODE_BREATH,
			[this](uint8_t val) {
				ESP_LOGI(TAG, "seg %d mode = %d", id, val);
				output.leds.setMode(id, val);
				output.sync_segments(id, [val](Segment& seg) {
					seg.mode = val;
				});
			}),
	speed(prefix + "_speed", "Animation speed", (id+1)*10+2,
			128,
			[this](uint16_t val) {
				ESP_LOGI(TAG, "seg %d speed = %d", id, val);
				output.leds.setSpeed(id, convert_speed(val));
				output.sync_segments(id, [val](Segment& seg) {
					seg.speed = val;
				});
			}),
	colors {
		ControlRGB(prefix + "_color1", "Color 1",  (id+1)*10+3,
				RgbColor{255, 0, 0},
				std::bind(&Output::set_color, output, id, 0, _1)),
		ControlRGB(prefix + "_color2", "Color 2", (id+1)*10+4,
				RgbColor{0, 255, 0},
				std::bind(&Output::set_color, output, id, 1, _1)),
		ControlRGB(prefix + "_color3", "Color 3", (id+1)*10+5,
				RgbColor{0, 0, 255},
				std::bind(&Output::set_color, output, id, 2, _1))
	},
	next(prefix + "_next", "Next mode", (id+1)*10+6,
			[this](bool val) {
				ESP_LOGI(TAG, "seg %d next", id);
				auto m = output.leds.getMode(id);
				mode = m == MODE_COUNT ? 0 : m + 1;
			}),
	prev(prefix + "_prev", "Prev mode", (id+1)*10+7,
			[this](bool val) {
				ESP_LOGI(TAG, "seg %d prev", id);
				auto m = output.leds.getMode(id);
				mode = m == 0 ? MODE_COUNT : m - 1;
			})
{}

Output::Output(uint16_t num_leds, uint8_t pin, neoPixelType type) :
	leds(num_leds, pin, type),
	artnet("artnet", "ArtNet enabled", 5, true,
		[](bool val) {
			ESP_LOGI(TAG, "artnet = %d", val);
		}),
	brightness("brightness", "Brightness", 6, 255,
		[this](uint8_t val) {
			leds.setBrightness(val);
		}),
	sync("sync", "Sync", 8, true,
		[this](bool val) {
			ESP_LOGI(TAG, "sync = %d", val);
			sync_segments(0, [this](Segment& seg) {
				const auto& src = segments[0];
				seg.mode = src.mode;
				seg.speed = src.speed;
				for (uint8_t i = 0; i < MAX_NUM_COLORS; i++) {
					seg.colors[i] = src.colors[i];
				}
			});
		}),
	trigger("trigger", "Trigger", 7,
		[this](bool val) {
			leds.trigger();
		})
{
}

void Output::sync_segments(uint8_t seg, std::function<void(Segment&)> func)
{
	if (sync_in_progress || !sync)
		return;
	sync_in_progress = true;
	for (auto& item : segments) {
		if (item.id == seg)
			continue;
		func(item);
	}
	sync_in_progress = false;
}

void Output::set_color(uint8_t seg, uint8_t n, const RgbColor& val) {
	auto c = convert_color(val);
	ESP_LOGI(TAG, "seg %d color %d = %x", seg, n, HtmlColor(c).Color);
	leds.setColor(seg, n, c);
	sync_segments(seg, [n, val](Segment& seg) {
		seg.colors[n] = val;
	});
}

}
