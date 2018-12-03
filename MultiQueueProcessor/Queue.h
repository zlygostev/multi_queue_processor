#pragma once
#include <queue>
#include <mutex>
#include "IQueue.h"

template<typename Value, size_t MaxQueueCapacity>
struct Queue : IQueue<Value, MaxQueueCapacity>
{
	using QueueType = IQueue<Value, MaxQueueCapacity>;

	Queue(QueueType::NotifySubscriberT notificationSubscriber) : IQueue(notificationSubscriber)
	{}

	virtual ~Queue() = default;

	void Enqueue(Value&& value) override
	{
		std::lock_guard<decltype(m_mutex)> lock{ m_mutex };
		if (m_queue.size() >= m_maxCapacity)
			throw std::overflow_error("Max capacity of queue is reached");

		m_queue.emplace(std::move(value));
		//TODO lock could be free if m_notifySubscriber supports multithreading
		m_notifySubscriber();
	}

	bool Dequeue(Value& value) override
	{
		std::lock_guard<decltype(m_mutex)> lock{ m_mutex };
		if (m_queue.empty())
		{
			return false;
		}
		value = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	bool IsEmpty() override
	{
		std::lock_guard<decltype(m_mutex)> lock{ m_mutex };
		return m_queue.empty();
	}
private:
	//TODO think about boost lock free queue instead of this one
	std::queue<Value> m_queue;
	std::mutex m_mutex;
};