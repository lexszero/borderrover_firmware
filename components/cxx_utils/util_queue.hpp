#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <chrono>
#include <optional>
#include <stdexcept>

template <typename Item>
class Queue
{
	public:
		Queue(size_t size)
		{
			handle = xQueueCreate(size, sizeof(Item));
			if (handle == NULL)
				throw std::runtime_error("Failed to create a queue");
		}

		~Queue()
		{
			vQueueDelete(handle);
		}

		bool receive(Item& item, TickType_t timeout = 0)
		{
			return !!xQueueReceive(handle, &item, timeout);
		}

		std::optional<Item> receive(TickType_t timeout = 0)
		{
			Item item;
			if (receive(item, timeout))
				return item;
			else
				return std::nullopt;
		}

		bool send(const Item&& item, TickType_t timeout = 0)
		{
			auto ret = xQueueSend(handle, &item, timeout);
			return !!ret;
		}

	private:
		QueueHandle_t handle;
};
