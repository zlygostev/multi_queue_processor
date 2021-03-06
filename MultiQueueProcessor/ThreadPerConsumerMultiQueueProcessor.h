﻿#pragma once
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <stdexcept>
#include "IMultiQueueProcessor.h"
#include "Queue.h"
#include "SingleThreadQueueProcessor.h"

const static size_t MAX_QUEUE_CAPACITY = 1000;
const static size_t MAX_QUEUES_CAPACITY = 512;

template<typename QueueProcessorT>
struct QueueNode
{
	QueueNode(QueueNode&& q) noexcept
	{
		queueProcessor = std::move(q.queueProcessor);
	}

	QueueNode(std::chrono::milliseconds jobsWaitingTimeout)
	{
		queueProcessor = make_unique<QueueProcessorT>(jobsWaitingTimeout);
	}
	std::mutex mtx;
	std::unique_ptr<QueueProcessorT> queueProcessor;
};

template<typename Key, typename Value, 
	size_t MaxQueueCapacity = MAX_QUEUE_CAPACITY,
	size_t MaxQueuesCapacity = MAX_QUEUES_CAPACITY,
	typename Queue = LockedQueueWithNotifications<Key, MaxQueueCapacity>,
	typename QueueProcessorT = SingleQueueProcessor<Key, Value, Queue>>
struct ThreadPerConsumer_MultiQueueProcessor : IMultiQueueProcessor<Key, Value>
{

	using QueueNodeT = QueueNode<QueueProcessorT>;

	ThreadPerConsumer_MultiQueueProcessor(std::chrono::milliseconds jobsWaitingTimeout = std::chrono::milliseconds(2000))
			:m_running(true), m_jobsWaitingTimeout(jobsWaitingTimeout)
	{}

	virtual ~ThreadPerConsumer_MultiQueueProcessor()
	{
		StopProcessing();
	};

	void StopProcessing() override
	{
		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
		m_running = false;
		for (auto iter = m_multiQueue.begin(); iter != m_multiQueue.end(); ++iter)
		{
			std::lock_guard<decltype(iter->second.mtx)> queueLock{ iter->second.mtx };
			iter->second.queueProcessor.reset(nullptr);
		}
	}

	void Subscribe(Key id, IConsumer<Key, Value> * consumer) override
	{
		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
		if (!m_running)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor:Subscribe Attempt of run a method of the object when it is already stopped.");
		}
		auto iter = m_multiQueue.find(id);
		if (iter == m_multiQueue.end())
		{
			if (m_multiQueue.size() > MaxQueuesCapacity)
			{
				throw std::overflow_error("Subscribe: Max capacity of queues is reached");
			}
			auto insertedState = m_multiQueue.insert(std::make_pair(id, QueueNodeT(m_jobsWaitingTimeout)));
			iter = insertedState.first;
		}
		multiQueueLock.unlock();
		std::lock_guard<decltype(iter->second.mtx)> queueLock{ iter->second.mtx };
		if (!iter->second.queueProcessor)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor:Subscribe Attempt of run a method of the object when it is already stopped.");
		}
		iter->second.queueProcessor->Subscribe(id, consumer);
	}

	void Unsubscribe(Key id) override
	{
		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
		if (!m_running)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor:Unsubscribe Attempt of run a method of the object when it is already stopped.");
		}
		auto iter = m_multiQueue.find(id);
		if (iter == m_multiQueue.end())
		{
			//A queue with the Key is not found 
			return;
		}
		multiQueueLock.unlock();
		std::lock_guard<std::mutex> lock{ iter->second.mtx };
		if (iter->second.queueProcessor)
		{
			iter->second.queueProcessor->Unsubscribe();
		}
	};

	void Enqueue(Key id, Value value) override
	{
		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
		if (!m_running)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor:Enqueue Attempt of run a method of the object when it is already stopped.");
		}
		auto iter = m_multiQueue.find(id);
		if (iter == m_multiQueue.end())
		{
			if (m_multiQueue.size() > MaxQueuesCapacity)
			{
				throw std::overflow_error("Max capacity of queues is reached");
			}
			auto insertedState = m_multiQueue.emplace(make_pair(static_cast<const Key>(id), QueueNodeT(m_jobsWaitingTimeout)));
			iter = insertedState.first;
		}
		multiQueueLock.unlock();
		std::lock_guard<decltype(iter->second.mtx)> queueLock{ iter->second.mtx };
		if (!iter->second.queueProcessor)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor: Attempt of run a method of the object when it is already stopped.");
		}
		iter->second.queueProcessor->Enqueue(std::move(value));
	}

	Value Dequeue(Key id) override
	{
		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
		if (!m_running)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor: Attempt of run a method of the object when it is already stopped.");
		}

		auto iter = m_multiQueue.find(id);
		if (iter == m_multiQueue.end())
		{
			//A queue with the Key is not found 
			throw std::invalid_argument("The queue is not found");
		}

		multiQueueLock.unlock();
		std::lock_guard<decltype(iter->second.mtx)> queueLock{ iter->second.mtx };
		if (!iter->second.queueProcessor)
		{
			throw std::runtime_error("ThreadPerConsumer_MultiQueueProcessor: Attempt of run a method of the object when it is already stopped.");
		}
		return iter->second.queueProcessor->Dequeue();
	}

private:
	bool m_running;
	std::chrono::milliseconds m_jobsWaitingTimeout;
	std::mutex m_multiQueueMutex;//Lock of multi queue level
	std::map<Key, QueueNodeT> m_multiQueue;
};

