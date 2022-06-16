#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "body_control.hpp"

#include <thread>

#define TAG "bc"

std::shared_ptr<BodyControl> BodyControl::singleton_instance = nullptr;

OutputGPIO BodyControl::State::outs[] = {
//    Name		GPIO			active_low	open_drain
	{"lockout",	GPIO_NUM_23,	true,		true},
	{"valve0",	GPIO_NUM_5,		false,		false},
	{"valve1",	GPIO_NUM_17,	false,		false},
	{"valve2",	GPIO_NUM_16,	false,		false},
	{"pump",	GPIO_NUM_4,		false,		false},
	{"igniter",	GPIO_NUM_19,	true,		true},	
	{"aux0",	GPIO_NUM_22,	true,		true},
	{"aux1",	GPIO_NUM_18,	true,		true},
};

OutputId from_string(const std::string& s)
{
	int idx = 0;
	for (const auto& item : BodyControl::State::outs) {
		if (strcmp(item.name, s.data()) == 0) {
			return static_cast<OutputId>(idx);
		}
		idx++;
	}
	throw std::invalid_argument("Invalid output " + s);
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

BodyControl::State::State() :
	timestamp(system_clock::now()),
	mode(Mode::Disabled)
{
	ESP_LOGI(TAG, "resetting outputs");
	for (auto& out : outs) {
		out.set(false);
	}
}

void BodyControl::State::print() const
{
	std::cout << "\n\nTime: " << timestamp << ", mode " << mode << "\nOutputs: \n";
	for (const auto& out : outs) {
		std::cout << std::setfill(' ') << std::setw(10) << out.name << " : "
			<< (out.get() ? "\e[1;33mON\e[0m" : "OFF") << std::endl;
	}
}

BodyControl::BodyControl() :
	Task::Task(TAG, 8*1024, 15),
	Lockable(TAG),
	lockout("lockout", "Safety lockout", 10, false,
		[this](bool val) {
			set_output(Lockout, val);
		}),

	state()
{
	singleton_instance = std::shared_ptr<BodyControl>(this);
	Task::start();
	register_console_cmd();
}

BodyControl& BodyControl::instance()
{
	return *BodyControl::singleton_instance.get();
}

void BodyControl::reset()
{
	auto lock = take_unique_lock();
	state = State();
	notify();
}

void BodyControl::set_output(OutputId id, bool on)
{
	auto lock = take_unique_lock();
	state.outs[to_underlying(id)].set(on);
	notify();
}

void BodyControl::pulse_output(OutputId id, TickType_t length)
{
}

void BodyControl::test_outputs()
{
	using namespace std::chrono;
	using std::this_thread::sleep_for;
	auto lock = take_unique_lock();
	ESP_LOGI(TAG, "Testing outputs");
	for (auto& out : state.outs) {
		state.outs[OutputId::Lockout].set(false);
		out.set(true);
		notify();

		sleep_for(500ms);

		state.outs[OutputId::Lockout].set(true);
		sleep_for(500ms);
		out.set(false);

		sleep_for(milliseconds(500));
	}
}

void BodyControl::print_state() const
{
	auto lock = take_shared_lock();
	state.print();
}

void BodyControl::run()
{
	ESP_LOGI(TAG, "started");
	while (1) {
		auto event = events.wait(Event::Any, 1000 / portTICK_PERIOD_MS, true, false);
		if (event & Event::StateUpdate) {
			print_state();
		}
	}
}

void BodyControl::notify()
{
	state.timestamp = system_clock::now();
	events.set(Event::StateUpdate);
}
