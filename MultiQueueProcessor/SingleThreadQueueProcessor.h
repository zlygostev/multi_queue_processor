#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>
#include <exception>
#include <condition_variable>

template<typename Key, typename Value, typename Queue>
struct ISingleQueueProcessor
{
	virtual ~ISingleQueueProcessor() = default;
	
	virtual void Subscribe(const Key& key, IConsumer<Key, Value> * consumer) = 0;

	virtual void Unsubscribe() = 0;

	virtual void Enqueue(Value&& value) = 0;

	virtual bool Dequeue(Value&) = 0;

};

template<typename Key, typename Value, typename Queue>
struct SingleQueueProcessor : ISingleQueueProcessor< Key, Value, Queue>
{

	SingleQueueProcessor(std::chrono::milliseconds jobsWaitingTimeout) : m_isStop(true), 
		 m_consumer(nullptr), m_jobsWaitingTimeout(jobsWaitingTimeout)
	{
		m_queue = make_unique<Queue>(std::bind(&SingleQueueProcessor::NewEventCome, this));
	}

	virtual ~SingleQueueProcessor()
	{
		Unsubscribe();
		//Wait end of Enqueue and Deque. Possibly inside? No
	}

	void Subscribe(const Key& key, IConsumer<Key, Value> * consumer)
	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);
		if (m_consumer)
		{
			throw std::invalid_argument("The queue already has consumer");
		}
		if (!consumer)
		{
			throw std::invalid_argument("Consumer should not be null");
		}
		auto pKey = new Key(key);
		m_key.reset(pKey);
		m_consumer = consumer;
		//Start Job
		m_isStop = false;
		m_thread = make_unique<std::thread>(std::bind(&SingleQueueProcessor::Proccess, this));
	}

	void Unsubscribe()
	{
		m_isStop = true;
		{
			std::lock_guard<decltype(m_jobsWaitingMutex)> jobsWaitingLock{ m_jobsWaitingMutex };
			m_jobsWaitingCV.notify_all();
		}
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);
		if (m_thread)
		{
			m_thread->join();
			m_thread.reset(nullptr);
		}
		m_consumer = nullptr;
	}

	void Enqueue(Value&& value)
	{
		m_queue->Enqueue(std::move(value));
	}

	bool Dequeue(Value& val)
	{
		return m_queue->Dequeue(val);
	}
private:
	bool WakeUpCondition()
	{
		return !m_queue->IsEmpty() || m_isStop;
	}

	void WaitNewEvent()
	{
		do{
			std::unique_lock<decltype(m_jobsWaitingMutex)> lock(m_jobsWaitingMutex);
			m_jobsWaitingCV.wait_for(lock, m_jobsWaitingTimeout, std::bind(&SingleQueueProcessor::WakeUpCondition, this));
		} while (!WakeUpCondition());
	}

	void NewEventCome() 
	{
		std::lock_guard<decltype(m_jobsWaitingMutex)> lock(m_jobsWaitingMutex);
		m_jobsWaitingCV.notify_one();
	}

	//TODO Move Process in some Worker Class. And request object of Worker from pool of working threads
	void Proccess()
	{
		Value val;
		while (!m_isStop)
		{
			if (!m_queue->Dequeue(val))
			{
				WaitNewEvent();
				continue;
			}

			try
			{
				std::unique_lock<decltype(m_mutex)> lock(m_mutex);
				if (!m_consumer)
					continue;

				m_consumer->Consume(*(m_key.get()), val);
			}
			catch (const std::exception&)
			{
				//log ex.what()
			}
			catch (...)
			{
				//ERR message and stop. Need to know what could be raised except std::exceptions
				m_isStop = true;
			}

		}
	}

	std::atomic<bool> m_isStop; 
	std::mutex m_mutex;
	std::unique_ptr<Queue> m_queue;// Thread safe queue
	
	std::unique_ptr <Key> m_key;
	// IConsumer couldn't be deleted in this class because  
	// 1. there is no information of memory management for IConsumer 
	// 2. there is no virtual destructor in interface - so there is no ability to delete here interaface implementation
	IConsumer<Key, Value> * m_consumer;
	std::mutex m_jobsWaitingMutex;
	std::condition_variable m_jobsWaitingCV;
	std::chrono::milliseconds m_jobsWaitingTimeout;
	std::unique_ptr<std::thread> m_thread;
};
