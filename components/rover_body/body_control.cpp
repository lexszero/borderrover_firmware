#include "body_control.hpp"

#include "core_status_led.hpp"
#include "driver/gpio.h"

#include <thread>
#include <tuple>

#define TAG "bc"

using namespace esp_now;

static const PeerAddress PeerRemote(0x0c, 0xb8, 0x15, 0xf6, 0x6a, 0xed);

std::shared_ptr<BodyControl> BodyControl::singleton_instance = nullptr;

std::array<OutputGPIO, OutputId::_TotalCount> BodyControl::State::gpios = {{
	// Name		GPIO			active_low	open_drain
	{"valve0",	GPIO_NUM_5,		false,		false},
	{"valve1",	GPIO_NUM_17,	false,		false},
	{"valve2",	GPIO_NUM_16,	false,		false},
	{"pump",	GPIO_NUM_4,		false,		false},
	{"igniter",	GPIO_NUM_19,	true,		true},
	{"aux0",	GPIO_NUM_22,	true,		true},
	{"aux1",	GPIO_NUM_18,	true,		true},
	{"lockout",	GPIO_NUM_23,	true,		true}
}};

using std::chrono::microseconds;
using std::chrono::time_point_cast;
using std::chrono::duration_cast;

OutputId from_string(const std::string& s)
{
	return BodyControl::instance().get_output_id(s);
}

static constexpr auto mode_lookup_table = make_array<>(
        std::tuple(BodyControl::Mode::Disabled, "Disabled"),
        std::tuple(BodyControl::Mode::Direct, "Direct"),
        std::tuple(BodyControl::Mode::Pulse, "Pulse"),
        std::tuple(BodyControl::Mode::Burst, "Burst"),
        std::tuple(BodyControl::Mode::Remote, "Remote")
        );

std::ostream& operator<<(std::ostream &strm, const BodyControl::Mode &mode)
{
    return strm << lookup<>(mode_lookup_table,
                mode,
                "UNKNOWN");
}

std::ostream& Core::operator<<(std::ostream& os, const Core::InputGPIO& gpio)
{
	os << std::setfill(' ') << std::setw(10) << gpio.name << " : "
			<< (gpio.get() ? "\e[1;33mON\e[0m" : "OFF");
	return os;
}

std::ostream& operator<<(std::ostream& os, const PulsedOutput& out)
{
	os << out.gpio;
	return os;
}

PulsedOutput::PulsedOutput(OutputGPIO& gpio, int order,
		duration min_pulse_length,
		duration max_pulse_length,
		duration min_pause_length) :
	Lockable(gpio.name),
	name(gpio.name),
	gpio(gpio),
	min_pulse_length(min_pulse_length),
	max_pulse_length(max_pulse_length),
	min_pause_length(min_pause_length),
	t_last_on(), t_last_off(),
	timer_single([this](void) {
				auto lock = take_unique_lock();
				reset(lock);
				running_single = false;
			}, name + "_single"),
	timer_periodic([this](void) {
				if (enable)
					trigger_single.set(true);
			}, name + "_periodic"),
	running_single(false),
	t_on(name + "_t_on", "On time, x10ms", order + 1, 10,
			[this](const uint32_t val) {
				ESP_LOGI(name.c_str(), "Set pulse on time %d", val);
			}),
	period(name + "_period", "Pulse period, x10ms", order + 2, 0,
			[this](const uint32_t val) {
				ESP_LOGI(name.c_str(), "Set pulse repetition period %d", val);
			}),
	trigger_single(name + "_trigger", "Trigger single pulse", order + 3,
			[this](const bool val) {
				auto lock = take_unique_lock();
				if (val)
					pulse_single(lock, milliseconds(static_cast<uint32_t>(t_on)*10));
			}),
	enable(name + "_enable", "Enable output or pulsing if period>0", order + 4, false,
			[this](const bool val) {
				set(val);
			})
{
	ESP_LOGI(name.c_str(), "PulsedOutput: min_pulse=%lldms, max_pulse=%lldms, min_pause=%lldms",
			min_pulse_length.count(),
			max_pulse_length.count(),
			min_pause_length.count());
}

void PulsedOutput::reset()
{
	auto lock = take_unique_lock();
	reset_periodic(lock);
	reset_single(lock);
	reset(lock);
}

bool PulsedOutput::get()
{
	auto lock = take_shared_lock();
	return gpio.get();
}

void PulsedOutput::set(bool new_state)
{
	auto lock = take_unique_lock();

	auto now = time_now();

	reset_single(lock);

	if (new_state) {
		if (period) {
			pulse_periodic(lock,
					milliseconds(static_cast<uint32_t>(t_on) * 10),
					milliseconds(static_cast<uint32_t>(period) * 10));
		}
		else {
			gpio.set(new_state);
			t_last_on = now;
			if (max_pulse_length.count()) {
				timer_single.start(duration_cast<microseconds>(max_pulse_length));
				running_single = true;
			}
		}
	}
	else {
		reset_periodic(lock);
		reset_single(lock);
		reset(lock);
	}
}

void PulsedOutput::reset(const PulsedOutput::unique_lock& lock)
{
	(void)lock;

	gpio.set(false);
	t_last_off = time_now();
}

void PulsedOutput::reset_single(const unique_lock& lock)
{
	(void)lock;

	if (running_single) {
		try {
			timer_single.stop();
		}
		catch (...) {}
	}
	running_single = false;
}

void PulsedOutput::reset_periodic(const unique_lock& lock)
{
	(void)lock;

	if (running_periodic) {
		try {
			timer_periodic.stop();
		}
		catch (...) {}
	}
	running_periodic = false;
}

void PulsedOutput::pulse_single(const PulsedOutput::unique_lock& lock, const duration& length)
{
	duration t_on = length;
	if (t_on < min_pulse_length)
		t_on = min_pulse_length;
	else if (max_pulse_length.count() && (t_on > max_pulse_length))
		t_on = max_pulse_length;

	ESP_LOGI(name.c_str(), "pulse_single %lldms", t_on.count());

	reset_single(lock);

	gpio.set(true);
	t_last_on = time_now();
	
	timer_single.start(std::chrono::duration_cast<microseconds>(t_on));
	running_single = true;
}

void PulsedOutput::pulse_periodic(const PulsedOutput::unique_lock& lock, const duration& length, const duration& period)
{
	duration t_on = length;
	duration t_repeat = period;
	if (t_on < min_pulse_length)
		t_on = min_pulse_length;
	else if (max_pulse_length.count() && (t_on > max_pulse_length))
		t_on = max_pulse_length;
	if (t_repeat < t_on + min_pause_length)
		t_repeat = t_on + min_pause_length;

	ESP_LOGI(name.c_str(), "pulse_periodic t_on=%lldms, t_repeat=%lldms", t_on.count(), t_repeat.count());
	pulse_single(lock, t_on);
	reset_periodic(lock);
	timer_periodic.start_periodic(duration_cast<microseconds>(t_repeat));
	running_periodic = true;
}

duration PulsedOutput::relative_pulse_length(uint32_t percent)
{
	return duration_cast<duration>(min_pulse_length + (max_pulse_length - min_pulse_length) * static_cast<double>(percent) / 100.0);
}

BodyControl::State::State(BodyControl& bc) :
	timestamp(time_now()),
	mode(Mode::Disabled),
	lockout(gpios[to_underlying(OutputId::Lockout)]),
	outs {			//	GPIO			start_id	min_pulse	max_pulse	min_pause
		PulsedOutput {	gpios[Valve0],	10,			50ms,		10s},
		PulsedOutput {	gpios[Valve1],	20,			50ms,		10s},
		PulsedOutput {	gpios[Valve2],	30,			50ms,		10s},
		PulsedOutput {	gpios[Pump],	40,			200ms,		5s},
		PulsedOutput {	gpios[Igniter],	50,			300ms,		0ms},
		PulsedOutput {	gpios[Aux0],	60,			20ms,		0ms},
		PulsedOutput {	gpios[Aux1],	70,			100ms,		5s}
	}
{
	ESP_LOGI(TAG, "resetting outputs");
	lockout.set(false);
	for (auto& gpio : gpios) {
		gpio.set(false);
		gpio.on_change([&bc](bool new_state) {
				bc.notify();
			});
	}
}

void BodyControl::State::print() const
{
	std::cout << "\n\nTime: " << timestamp << ", mode " << mode <<
		"\n" << lockout <<
		"\nOutputs: \n";
	for (const auto& out : outs) {
		std::cout << out << std::endl;
	}
	std::cout << std::endl;
}

BodyPackedState BodyControl::State::pack() const
{
	BodyPackedState st;
	st.lockout = lockout.get();
	return st;
}

OutputGPIO& BodyControl::State::get_gpio(OutputId output)
{
	if (output == OutputId::Lockout)
		return lockout;
	else
		return gpios[to_underlying(output)];
}

OutputId BodyControl::State::get_output_id(const std::string& name)
{
	int idx = 0;
	for (const auto& item : gpios) {
		if (strcmp(item.name, name.data()) == 0) {
			return static_cast<OutputId>(idx);
		}
		idx++;
	}
	throw std::invalid_argument("Invalid output " + name);
}

BodyControl::BodyControl() :
	Task::Task(TAG, 8*1024, 15),
	Lockable(TAG),
	lockout("lockout", "Safety lockout", 1, false,
		[this](bool val) {
			set_output(Lockout, val);
		}),
	state(std::make_unique<State>(*this)),
	state_update_callback(nullptr),
	leds(32*8, GPIO_NUM_13, 0),
	led_link(std::make_shared<Core::StatusLed>("led_link",
		OutputGPIO("led_link", GPIO_NUM_26))),
	events()
{
	singleton_instance = std::shared_ptr<BodyControl>(this);

	espnow->set_led(led_link);
	espnow->add_peer(PeerBroadcast, std::nullopt, DEFAULT_WIFI_CHANNEL);
	espnow->add_peer(PeerRemote, std::nullopt, DEFAULT_WIFI_CHANNEL);
	espnow->on_recv(esp_now::MessageId::Announce,
		[this](const Message& msg) {
			auto peer = msg.peer();
			ESP_LOGD(TAG, "Announce from %s", to_string(peer).c_str());
			if (peer == PeerRemote) {
				remote_last_message_time = time_now();
			}
		});
	espnow->on_recv(static_cast<MessageType>(RoverRemoteState),
		[this](const Message& msg) {
			handle_remote_state(msg.payload_as<const RemoteState>());
		});

	leds.leds.setNumSegments(1);
	leds.segments.emplace_back(leds, "led", 0, 0, 32*8);
	leds.start();
	Task::start();
	register_console_cmd();
}

BodyControl& BodyControl::instance()
{
	return *BodyControl::singleton_instance.get();
}

OutputId BodyControl::get_output_id(const std::string& name)
{
	return state->get_output_id(name);
}

void BodyControl::reset()
{
	auto lock = take_unique_lock();
	state = std::make_unique<State>(*this);
	notify();
}

bool BodyControl::set_output(OutputId id, bool on)
{
	return set_output(take_unique_lock(), id, on);
}

bool BodyControl::set_output(const unique_lock& lock, OutputId id, bool on)
{
	(void)lock;

	bool changed = false;
	if (id == OutputId::Lockout) {
		if (state->lockout.get() != on) {
			state->lockout.set(on);
			state->lockout_change_time = time_now();
			changed = true;
		}
	}
	else {
		auto& out = state->outs[to_underlying(id)];
		if (out.get() != on) {
			out.enable.set(on);
			changed = true;
		}
	}
	return changed;
}

void BodyControl::pulse_output(OutputId id, uint8_t length)
{
	if (id >= OutputId::_PulsedCount)
		throw std::runtime_error("Output %d doesn't support pulsing");
	auto lock = take_unique_lock();
	auto& out = state->outs[to_underlying(id)];
	out.t_on.set(length);
	out.trigger_single.set(true);
}

void BodyControl::test_outputs()
{
	using namespace std::chrono;
	using std::this_thread::sleep_for;
	auto lock = take_unique_lock();
	ESP_LOGI(TAG, "Testing outputs");
	for (auto& out : state->outs) {
		state->get_gpio(OutputId::Lockout).set(false);
		out.set(true);

		sleep_for(500ms);

		state->get_gpio(OutputId::Lockout).set(false);
		sleep_for(500ms);
		out.set(false);

		sleep_for(milliseconds(500));
	}
}

void BodyControl::print_state() const
{
	auto lock = take_shared_lock();
	state->print();
}

void BodyControl::run()
{
	ESP_LOGI(TAG, "started");
	Core::status_led->blink(500);
	bool need_send_state = false;
	while (1) {
		const auto event = events.wait(Event::Any, 10 / portTICK_PERIOD_MS, true, false);
		const auto now = time_now();
		ESP_LOGD(TAG, "event 0x%08x", to_underlying(event));
		if (event & Event::StateUpdate) {
			Core::status_led->blink_once(50);
			print_state();
			need_send_state = true;
		}

		auto since_last_state_send = now - last_state_send_time;
		if ((since_last_state_send > 500ms) ||
			(need_send_state && (since_last_state_send > 10ms))) {
			try {
				espnow->send(MessageRoverBodyState(PeerBroadcast, state->pack()));
				last_state_send_time = now;
				need_send_state = false;
			}
			catch (const std::exception& e) {
				ESP_LOGE(TAG, "state send failed: %s", e.what());
			}
		}
	}
}

void BodyControl::notify()
{
	state->timestamp = time_now();
	events.set(Event::StateUpdate);
}

void BodyControl::remote_button_direct_mapping(const unique_lock& lock, const RemoteState& st, RemoteButton btn, OutputId output)
{
	(void)lock;
	set_output(lock, output, st.is_pressed(btn));
}

void BodyControl::handle_remote_state(const RemoteState& st)
{
	ESP_LOGI(TAG, "%s", to_string(st).c_str());
	auto lock = take_unique_lock();
	auto now = time_now();
	remote_last_message_time = now;
	remote_state_time = now;
	if (st.is_pressed(RemoteButton::SwitchRed)) {
		set_output(lock, OutputId::Lockout, true);
		remote_button_direct_mapping(lock, st, RemoteButton::Left, OutputId::Valve0);
		remote_button_direct_mapping(lock, st, RemoteButton::Right, OutputId::Valve1);
		remote_button_direct_mapping(lock, st, RemoteButton::Up, OutputId::Aux0);
		remote_button_direct_mapping(lock, st, RemoteButton::Down, OutputId::Aux1);
	}
	else {
		set_output(lock, OutputId::Lockout, false);
	}
}
