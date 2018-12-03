#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>



//Use it if number of queues in not too much big
template<typename Key, typename Value>
struct QueueProcessor : IQueueProcessor<Key, Value>
{
	QueueProcessor(std::chrono::duration jobsWaitingTimeout) :
		m_isStop(false),
		m_hasItems(false),
		m_waiting_timeout(jobsWaitingTimeout),
		m_thread(std::bind(&QueueProcessor::work, this))
	{}

	virtual ~QueueProcessor()
	{
		Stop();
		m_thread.join();
	};

	void Process(Task* task) override
	{
		if (!task || task->queue == nullptr ||
			task->key == nullptr ||
			task->consumer == nullptr ||
			task->notification == nullptr)
		{
			throw std::invalid_argument("all QueueProcessor::Process parameters are requered to be fill");
		}
		if (task->maxCount != 0)
		{
			throw std::invalid_argument("Max count must be set to 0 for infinite loop by queue in this object.");
		}

		std::unique_lock<decltype(m_working_mutex)> lock{ m_working_mutex };
		m_task = make_unique<Task>(queue, key, consumer, maxCount, notification);
		m_hasItems = true;
		lock.unlock();

		m_waiting_cv.notify_one();// To start work
	}

	void Stop() override
	{
		m_isStop = true;
		m_waiting_cv.notify_one();

	};

	void NewItempNotification() override
	{
		m_waiting_cv.notify_one();

	};

private:
	struct Task
	{
		Task(IQueue<Value>* queue,
			Key* key,
			IConsumer<Key, Value>* consumer,
			size_t maxCount,
			ResultNotification notification
		) :
			queue(queue),
			key(key),
			consumer(consumer),
			maxCount(maxCount),
			notification(notification)
		{}

		IQueue<Value>* queue;
		Key* key;
		IConsumer<Key, Value>* consumer;
		size_t maxCount;
		ResultNotification notification;
	};


	void doTask()
	{
		std::unique_lock<decltype(m_working_mutex)> lock{ m_working_mutex };
		if (m_task)
		{
			Value value;

			while(!m_isStop)
			{
			
				if (!m_task->queue->Dequeue(value))
				{
					lock.unlock();
					std::unique_lock<decltype(m_waiting_mutex)> lock{ m_waiting_mutex };
					m_isReady = true;
					if (!m_waiting_cv.wait_for(lock, m_waiting_timeout, std::bind(&QueueProcessor::wakeupPrediction, this)))
					{
						//"timeout on waiting tasks"
					}
					else
					{
						m_isReady = false;
					}
					

					lock.lock();
				}
				m_task->consumer->Consume(m_task->key, value);
				if (m_isStop)
				{
					//request to stop process
					break;
				}
			}

			// task is done
			bool areAllItemProcessed = (i >= m_maxCount);

			m_task->notification(areAllItemProcessed, m_isStop);
			m_task.reset(nullptr);
		}
	}
	bool wakeupPrediction() const
	{
		return m_isStop || m_task;
	}
	void waitNewTask()
	{
		if (!m_isStop)
		{
			std::unique_lock<decltype(m_waiting_mutex)> lock{ m_waiting_mutex };
			m_isReady = true;
			if (!m_waiting_cv.wait_for(lock, m_waiting_timeout, std::bind(&QueueProcessor::wakeupPrediction, this)))
			{
				//"timeout on waiting tasks"
			}
			else
			{
				m_isReady = false;
			}


		}
	}

	void work()
	{
		while (!m_isStop)
		{
			doTask();
			waitNewTask();

		}
	}

private:
	//std::atomic<bool> m_isReady;//TODO: look if it's really need
	std::atomic<bool> m_isStop;
	std::atomic<bool> m_hasItems;
	std::mutex m_working_mutex;
	std::unique_ptr<Task> m_task;

	std::mutex m_waiting_mutex;
	std::condition_variable m_waiting_cv;
	std::chrono::duration m_waiting_timeout;
	std::thread m_thread;

	//IQueue<Value>* m_queue;
	//IConsumer<Key, Value>* m_consumer;
	//size_t m_maxCount;
	//ResultNotification m_notification;
};

//Use it if number of queues in not too much big
template<typename Key, typename Value>
struct FixedElementsQueueProcessor : IQueueProcessor<Key, Value>
{
	QueueProcessor(std::chrono::duration jobsWaitingTimeout) : 
		m_isReady(false), 
		m_isStop(false), 
		m_waiting_timeout(jobsWaitingTimeout),
		m_thread(std::bind(&QueueProcessor::work, this))
	{}

	virtual ~QueueProcessor()
	{
		Stop();
		m_thread.join();
	};

	void Process(Task* task) override
	{
		if (!task || task->queue == nullptr || 
			task->key == nullptr || 
			task->consumer == nullptr || 
			task->maxCount == nullptr || 
			task->notification == nullptr)
		{
			throw std::invalid_argument("all QueueProcessor::Process parameters are requered to be fill");
		}

		std::unique_lock<decltype(m_working_mutex)> lock{ m_working_mutex };
		m_task = make_unique<Task>(queue, key, consumer, maxCount, notification);
		lock.unlock();

		m_waiting_cv.notify_one();// To start work
	}

	void Stop() override
	{
		m_isStop = true;
		m_waiting_cv.notify_one();

	};

private:
	bool isReady() { return !m_isStop && m_isReady; }


	struct Task
	{
		Task(IQueue<Value>* queue,
			Key* key,
			IConsumer<Key, Value>* consumer,
			size_t maxCount,
			ResultNotification notification
		) :
			queue(queue),
			key(key),
			consumer(consumer),
			maxCount(maxCount),
			notification(notification)
		{}

		IQueue<Value>* queue;
		Key* key;
		IConsumer<Key, Value>* consumer;
		size_t maxCount;
		ResultNotification notification;
	};

	bool isNeedStopWork(size_t processedItems) 
	{
		
	}

	void doTask()
	{
		std::unique_lock<decltype(m_working_mutex)> lock{ m_working_mutex };
		if (m_task)
		{
			Value value;

			for (auto i = 0; isNeedStop(i); ++i)
			{
				if (!m_task->queue->Dequeue(value))
				{
					//task is processed. No more items
					break;
				}
				m_task->consumer->Consume(m_task->key, value);
				if (m_isStop)
				{
					//request to stop process
					break;
				}
			}

			// task is done
			bool areAllItemProcessed = (i >= m_maxCount);

			m_task->notification(areAllItemProcessed, m_isStop);
			m_task.reset(nullptr);
		}
	}
	bool wakeupPrediction() const
	{
		return m_isStop || m_task;
	}
	void waitNewTask()
	{
		if (!m_isStop)
		{
			std::unique_lock<decltype(m_waiting_mutex)> lock{ m_waiting_mutex };
			m_isReady = true;
			if (!m_waiting_cv.wait_for(lock, m_waiting_timeout, std::bind(&QueueProcessor::wakeupPrediction, this)))
			{
				//"timeout on waiting tasks"
			}
			else
			{
				m_isReady = false;
			}


		}
	}

	void work()
	{
		while (!m_isStop)
		{
			doTask();
			waitNewTask();

		}
	}

private:
	std::atomic<bool> m_isReady;//TODO: look if it's really need
	std::atomic<bool> m_isStop;
	std::mutex m_working_mutex;
	std::unique_ptr<Task> m_task;

	std::mutex m_waiting_mutex;
	std::condition_variable m_waiting_cv;
	std::chrono::duration m_waiting_timeout;
	std::thread m_thread;

	//IQueue<Value>* m_queue;
	//IConsumer<Key, Value>* m_consumer;
	//size_t m_maxCount;
	//ResultNotification m_notification;
};
