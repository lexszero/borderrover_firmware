#pragma once

#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

enum class EventEnum : uint8_t
{
	None = 0x00,
	Any = 0xff,
};

template <typename EventType>
class EventGroup
{
	public:
		using Event = EventType;
		
		EventGroup()
		{
			event = xEventGroupCreateStatic(&event_buf);
			if (!event)
				throw std::runtime_error("Unable to create event group");
		};

		void set(Event bit) {
			xEventGroupSetBits(
					event,
					static_cast<EventBits_t>(bit));
		};

		Event wait(Event mask, TickType_t timeout, bool clear = true, bool all = false) const
		{
			return static_cast<Event>(xEventGroupWaitBits(
					event,
					static_cast<EventBits_t>(Event::Any),
					clear,
					all,
					timeout));
		};

		using EventHandlerFn = std::function<Event(Event)>;

		void when(Event mask, TickType_t timeout, bool all, EventHandlerFn handler) const
		{
			auto ev = wait(mask, timeout, false, all);
			if (ev) {
				auto clear = handler(ev);
				if (clear)
					xEventGroupClearBits(event, static_cast<EventBits_t>(clear));
			}
		}

		void when_any(Event mask, TickType_t timeout, EventHandlerFn handler) const
		{
			when(mask, timeout, false, handler);
		}
		
		void when_any(TickType_t timeout, EventHandlerFn handler) const
		{
			when(Event::Any, timeout, false, handler);
		}

		void when_all(Event mask, TickType_t timeout, EventHandlerFn handler) const
		{
			when(mask, timeout, true, handler);
		}
		
	private:
		StaticEventGroup_t event_buf;
		EventGroupHandle_t event;
};
