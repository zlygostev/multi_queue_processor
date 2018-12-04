#pragma once
#include <queue>
#include <mutex>
#include "IQueue.h"

template<typename Value, size_t MaxQueueCapacity>
struct Queue : IQueue<Value, MaxQueueCapacity>
{
	using QueueType = IQueue<Value, MaxQueueCapacity>;

	virtual ~Queue() = default;

	void Enqueue(Value&& value) override
	{
		if (m_queue.size() >= m_maxCapacity)
			throw std::overflow_error("on enqueue: Max capacity of queue is reached");

		m_queue.emplace(std::move(value));
	}

	Value Dequeue() override
	{
		if (m_queue.empty())
			throw QueueIsEmpty();

		Value value(std::move(m_queue.front()));
		m_queue.pop();
		return std::move(value);
	}

	bool IsEmpty() override
	{
		return m_queue.empty();
	}
private:
	std::queue<Value> m_queue;
};

template<typename Value, size_t MaxQueueCapacity>
struct LockedQueueWithNotifications : IQueueWithNotifications<Value, MaxQueueCapacity>
{
	using QueueType = IQueueWithNotifications<Value, MaxQueueCapacity>;

	LockedQueueWithNotifications(QueueType::NotifySubscriberT notificationSubscriber) : IQueueWithNotifications(notificationSubscriber)
	{}

	virtual ~LockedQueueWithNotifications() = default;

	void Enqueue(Value&& value) override
	{
		std::lock_guard<decltype(m_mutex)> lock{ m_mutex };
		if (m_queue.size() >= m_maxCapacity)
			throw std::overflow_error("Max capacity of queue is reached");

		m_queue.emplace(std::move(value));
		//TODO lock could be free if m_notifySubscriber supports multithreading
		m_notifySubscriber();
	}

	Value Dequeue() override
	{
		std::lock_guard<decltype(m_mutex)> lock{ m_mutex };
		if (m_queue.empty())
			throw QueueIsEmpty();

		Value value(std::move(m_queue.front()));
		m_queue.pop();
		return std::move(value);
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