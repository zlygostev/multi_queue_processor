#pragma once
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <windows.h>
#include "IMultiQueueProcessor.h"

#define MaxCapacity 1000
template<typename Key, typename Value, size_t MaxQueueCapacity=MaxCapacity, typename Queue= Queue<Value, MaxQueueCapacity>>
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
		if (!running)
			throw std::runtime_error("Subscribe: Attempt to run a method of the object when it is already stopped.");
	
		auto iter = consumers.find(id);
		if (iter != consumers.end())
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
		if (!running)
			throw std::runtime_error("Enqueue: Attempt to run a method of the object when it is already stopped.");

		auto iter = queues.find(id);

		if (iter == queues.end())
		{
			auto res = queues.emplace(std::make_pair(std::move(id), Queue()));
			iter = res.first;
		}


		iter->second.Enqueue(std::move(value));
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

		return iter->second.Dequeue();
	}

protected:
	bool IsNeedToStopWait() 
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		bool result = false;
		for (auto consumerIter = consumers.begin(); consumerIter != consumers.end(); ++consumerIter)
		{
			auto queueIter = queues.find(consumerIter->first);
			if ((queueIter != queues.end()) && !queueIter->second.IsEmpty())
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
				auto queueIter = queues.find(consumerIter->first);
				if (queueIter != queues.end() && !queueIter->second.IsEmpty())
				{
					try
					{
						// Is it possibe to catch some exception here from Consume? There is no nothrow on interface
						consumerIter->second->Consume(queueIter->first, Dequeue(queueIter->first));
					}
					catch (const QueueIsEmpty&)
					{
						//Dequeue() could throw QueueIsEmpty if there is no more items in queue
						//INFO about it. But with global lock it shouldn't be happened.
					}
					catch (const std::exception&)
					{
						//log ex.what()
					}
					catch (...)
					{
						//ERR message and stop. Need to know what could be raised except std::exceptions
						running = false;
					}

				}
			}
		}
	}

protected:
	std::map<Key, IConsumer<Key, Value> *> consumers;
	std::map<Key, Queue> queues;

	std::mutex waitMutex;
	std::condition_variable waitCV;
	bool running;
	std::recursive_mutex mtx;
	std::thread th;
};
