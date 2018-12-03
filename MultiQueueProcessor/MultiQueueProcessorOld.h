#pragma once
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <windows.h>
#include "IMultiQueueProcessor.h"

#define MaxCapacity 1000
template<typename Key, typename Value>
class MultiQueueProcessorOld : public IMultiQueueProcessor<Key, Value>
{
public:
	MultiQueueProcessorOld() :
		running{ true },
		th(std::bind(&MultiQueueProcessorOld::Process, this)) {}

	virtual ~MultiQueueProcessorOld()
	{
		StopProcessing();
		th.join();
	}

	void StopProcessing()
	{
		running = false;
	}

	void Subscribe(Key id, IConsumer<Key, Value> * consumer)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = consumers.find(id);
		cout << "Subscribe by key " << id;
		if (iter != consumers.end())
		{
			throw std::logic_error("The queue with proposed key is already processing by another consumer.");
		}
		consumers.insert(std::make_pair(id, consumer));
	}

	void Unsubscribe(Key id)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = consumers.find(id);
		if (iter != consumers.end())
		{
			//TODO consumers  mem licks without delete?  
			consumers.erase(id);
		}
	}

	void Enqueue(Key id, Value value)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = queues.find(id);
		if (iter != queues.end())
		{
			if (iter->second.size() < MaxCapacity)
				iter->second.push_back(value);
		}
		else
		{
			queues.insert(std::make_pair(id, std::list<Value>()));
			iter = queues.find(id);
			if (iter != queues.end())
			{
				if (iter->second.size() < MaxCapacity)
					iter->second.push_back(value);
			}
		}
	}

	Value Dequeue(Key id)
	{
		std::lock_guard<std::recursive_mutex> lock{ mtx };
		auto iter = queues.find(id);
		if (iter != queues.end())
		{
			if (iter->second.size() > 0)
			{
				auto front = iter->second.front();
				iter->second.pop_front();
				return front;
			}
		}
		return Value{};
	}

protected:
	void Process()
	{
		while (running)
		{
			Sleep(10);
			std::lock_guard<std::recursive_mutex> lock{ mtx };
			for (auto iter = queues.begin(); iter != queues.end(); ++iter)
			{
				auto consumerIter = consumers.find(iter->first);
				if (consumerIter != consumers.end())
				{
					Value front = Dequeue(iter->first);
					if (front != Value{})
						consumerIter->second->Consume(iter->first, front);
				}
			}
		}
	}

protected:
	std::map<Key, IConsumer<Key, Value> *> consumers;
	std::map<Key, std::list<Value>> queues;

	bool running;
	std::recursive_mutex mtx;
	std::thread th;
};
