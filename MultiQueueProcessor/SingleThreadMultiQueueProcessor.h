#pragma once
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <windows.h>
#include "IMultiQueueProcessor.h"

#define MaxCapacity 1000
template<typename Key, typename Value, size_t MaxQueueCapacity=MaxCapacity>
class SingleThread_MultiQueueProcessor : public IMultiQueueProcessor<Key, Value>
{
public:
	SingleThread_MultiQueueProcessor() :
		running{ true },
		th(std::bind(&SingleThread_MultiQueueProcessor::Process, this)) {}

	virtual ~SingleThread_MultiQueueProcessor()
	{
		StopProcessing();
		th.join();
	}

	void StopProcessing()
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		running = false;
		waitCV.notify_one();
	}

	void Subscribe(Key id, IConsumer<Key, Value> * consumer)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = consumers.find(id);
		if (iter == consumers.end())
			throw std::logic_error("The queue with proposed key is already processing by another consumer.");

		if (!consumer)
			throw std::invalid_argument("Consumer should be not null");

		consumers.emplace(std::make_pair(std::move(id), std::move(consumer)));
		waitCV.notify_one();
	}

	void Unsubscribe(Key id)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = consumers.find(id);
		if (iter != consumers.end())
		{
			consumers.erase(id);
		}
	}

	void Enqueue(Key id, Value value)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = queues.find(id);

		if (iter == queues.end())
		{
			auto res = queues.emplace(std::make_pair(std::move(id), std::list<Value>()));
			iter = res.first;
		}

		if (iter->second.size() >= MaxCapacity)
		{
			throw std::overflow_error("on enqueue: Max capacity of queue is reached");
		}

		iter->second.push_back(std::move(value));
		waitCV.notify_one();
	}

	Value Dequeue(Key id)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = queues.find(id);
		if (iter == queues.end())
		{
			//A queue with the Key is not found 
			throw std::invalid_argument("Dequeue: The queue is not found");
		}

		if (iter->second.empty())
		{
			throw std::out_of_range("The queue is empty");
		}
		auto front = iter->second.front();
		iter->second.pop_front();
		return front;
	}

protected:
	bool IsNeedToStopWait() 
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		bool result = false;
		for (auto consumerIter = consumers.begin(); consumerIter != consumers.end(); ++consumerIter)
		{
			auto queueIter = queues.find(consumerIter->first);
			if ((queueIter != queues.end()) && !queueIter->second.empty())
			{
				result = true;
			}
		}
		return result;
	}

	void Process()
	{
		while (running)
		{
			{
				std::unique_lock<std::mutex> waitLock(waitMutex);
				waitCV.wait_for(waitLock, std::chrono::milliseconds(100), std::bind(&SingleThread_MultiQueueProcessor::IsNeedToStopWait, this));
			}
			std::lock_guard<std::recursive_mutex> lock{ mtx };
			for (auto consumerIter = consumers.begin(); consumerIter != consumers.end(); ++consumerIter)
			{
				try{
					auto queueIter = queues.find(consumerIter->first);
					if (queueIter != queues.end() && !queueIter->second.empty())
					{
						consumerIter->second->Consume(queueIter->first, Dequeue(queueIter->first));
					}
				}
				catch (const std::out_of_range& ex)
				{
					//INFO about it. But with global lock it shouldn't happen.
				}
			}
		}
	}

protected:
	std::map<Key, IConsumer<Key, Value> *> consumers;
	std::map<Key, std::list<Value>> queues;

	std::mutex waitMutex;
	std::condition_variable waitCV;
	bool running;
	std::recursive_mutex mtx;
	std::thread th;
};
