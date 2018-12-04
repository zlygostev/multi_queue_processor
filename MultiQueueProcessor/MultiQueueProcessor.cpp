// MultiQueueProcessor.cpp : Defines the entry point for the application.
//

#include "ThreadPerConsumerMultiQueueProcessor.h"
#include "SingleThreadMultiQueueProcessor.h"
#include "IConsumer.h"
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <assert.h>

using namespace std;

using ScheduleChart = std::map < std::string, std::chrono::milliseconds> ;

using BattleChart = std::map < std::string, std::pair<std::chrono::milliseconds, std::chrono::milliseconds>>;

template<typename Key, typename Value, size_t microseconds_consumption_time>
struct QuickConsumer: IConsumer<Key, Value>
{


	QuickConsumer() :m_consumedCount(0)
	{
	}

	void Consume(Key id, const Value &value)
	{
		++m_consumedCount;
		//do some work;
		//std::this_thread::sleep_for(std::chrono::microseconds(microseconds_consumption_time));
		{
			std::mutex mtx;
			std::unique_lock<std::mutex> lock(mtx);
			std::condition_variable cv;
			cv.wait_for(lock, std::chrono::microseconds(microseconds_consumption_time));
		}
		m_waitingCV.notify_one();
	}

	bool stopWaitCondition(size_t* itemsCount)
	{
		return *itemsCount < m_consumedCount;
	}

	bool WaitConsuming(size_t itemsCount, std::chrono::microseconds oneAttemptTimeout, std::chrono::microseconds timeout)
	{
		auto start = std::chrono::system_clock::now();
		while (itemsCount > m_consumedCount &&
			timeout > std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start))
		{
			std::unique_lock<decltype(m_waitingMutex)> lock(m_waitingMutex);
			m_waitingCV.wait_for(lock, oneAttemptTimeout, std::bind(&QuickConsumer::stopWaitCondition, this, &itemsCount));
		}
		return m_consumedCount >= itemsCount;
	}
	size_t ConsumedCount() const { return m_consumedCount; }
private:
	std::atomic<size_t> m_consumedCount;
	std::mutex m_waitingMutex;
	std::condition_variable m_waitingCV;
};



template <typename Key, typename Value>
struct ItemsGenerator
{
	Value GetValue(const Key& key);

	Key GetKey();
};

template <>
int ItemsGenerator<int, int>::GetValue(const int& key)
{
	static std::atomic<int> counter = 0;
	return 1500 * key + counter++;
}

template <>
int ItemsGenerator<int, int>::GetKey()
{
	static std::atomic<int> counter = 0;
	return ++counter;
}


template <typename Key, typename Value, typename Consumer, typename MQProcessor, typename ItemsGenerator>
struct SimpleFactory 
{
	IMultiQueueProcessor<Key, Value >* getMultiQueue()
	{
		return new MQProcessor();
	}

	Value getValue(const Key& key)
	{
		return m_generator.GetValue(key);
	}

	Key getKey()
	{
		return m_generator.GetKey();
	}

	std::unique_ptr<Consumer> getConsumer(const Key&)
	{ 
		return make_unique<Consumer>();
	}

	ItemsGenerator m_generator;
};


template<typename Key, typename Value, typename Consumer, typename Factory>
ScheduleChart OneQueue_Fullfill_And_WaitTillTheEndWithOneConsumer(Factory& factory, const size_t itemsCount)
{
	ScheduleChart chart;
	const auto key = factory.getKey();
	//std::vector<Value> values = factory.getValues(itemsCount);
	auto start = std::chrono::system_clock::now();
	std::unique_ptr<IMultiQueueProcessor<Key, Value>> mq(factory.getMultiQueue());
	auto startFullfill = std::chrono::system_clock::now();
	size_t i = 0;
	for (; i < itemsCount; ++i)
	{
		mq->Enqueue(key, std::move(factory.getValue(key)));
		if (i == 0)
		{
			auto firstItemInQueueTime = std::chrono::system_clock::now();
			chart["First item in queue"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startFullfill);
		}
	}
	chart["Queue is Fullfilled"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startFullfill);
	std::unique_ptr<Consumer> consumer = std::move(factory.getConsumer(key));
	auto startConsuming = std::chrono::system_clock::now();
	mq->Subscribe(key, consumer->get());
	if (!consumer.WaitConsuming(itemsCount, std::chrono::seconds(21)))
	{
		throw std::runtime_error("Items are not consumed at time");
	}
	chart["Items are consumed"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startConsuming);
	auto startOfStopping = std::chrono::system_clock::now();
	mq->StopProcessing();
	chart["MultiQueue stop"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startOfStopping);
	auto startDestroy = std::chrono::system_clock::now();
	mq.reset();
	chart["MultiQueue destroy"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startDestroy);
	return chart;
}

template<typename MultiQueueProcessor, size_t items_count>
ScheduleChart testOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer()
{
	auto factory = SimpleFactory<int, int , QuickConsumer<int, int>, MultiQueueProcessor, ItemsGenerator<int, int>>();
	size_t itemsCount = items_count;
	return OneQueue_Fullfill_And_WaitTillTheEndWithOneConsumer<int, int, QuickConsumer<int, int>, decltype(factory)>(
		factory, itemsCount);
}

template<typename MultiQueueProcessor1, typename MultiQueueProcessor2, size_t items_count>
BattleChart BattleTestOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer(const std::string& title)
{
	ScheduleChart schedule1 = testOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<MultiQueueProcessor1, 1000>();
	ScheduleChart schedule2 = testOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<MultiQueueProcessor2, 1000>();
	BattleChart result;
	std::cout << title <<  std::endl;
	for (auto item : schedule1)
	{
		result[item.first] = std::pair<decltype(item.second), decltype(item.second)>(schedule1[item.first], schedule2[item.first]);
		std::cout << item.first << ": " << schedule1[item.first].count() << " vs " << schedule2[item.first].count()  <<" milliseconds" << std::endl;
	}
	std::cout << std::endl;
	return result;

}

struct TestMQWorkerObserver
{
	TestMQWorkerObserver(size_t queuesCount, size_t queueCapacity, std::condition_variable& cv) :
		m_fullfilledCount(0),
		m_consumedCount(0),
		m_queuesCount(queuesCount),
		m_queueCapacity(queueCapacity),
		m_cv(cv)
	{ 
	}

	void OnFinishFullfilling(size_t queuesFullfilled)
	{
		m_fullfilledCount += queuesFullfilled;
		if (m_fullfilledCount >= m_queuesCount )
		{
			m_cv.notify_all();
		}
	}
	void OnFinishConsuming(size_t queuesConsumted)
	{
		m_consumedCount += queuesConsumted;
		if (m_consumedCount >= m_queuesCount)
		{
			m_cv.notify_all();
		}
	}
private:
	std::atomic<size_t> m_fullfilledCount;
	std::atomic<size_t> m_consumedCount;
	size_t m_queuesCount;
	size_t m_queueCapacity;
	std::condition_variable& m_cv;
};

enum WorkerState : uint8_t
{
	NOT_STARTED = 0,
	FULLFILLING,
	FULLFILLED,
	CONSUMING,
	CONSUMED
};
template<typename Key, typename Value, typename Consumer, typename Factory, typename MultiQueue>
struct TestMQWorker
{

	TestMQWorker(Factory& factory, MultiQueue& multiqueue, size_t queueCapacity, TestMQWorkerObserver& observer, size_t queues) :
		m_queueCapacity(queueCapacity),
		m_queuesCount(queues),
		m_factory(factory),
		m_observer(observer),
		m_multiqueue(multiqueue),
		m_state(WorkerState::NOT_STARTED)
	{
		m_thread = make_unique<std::thread>(std::bind(&TestMQWorker::Work, this));
	}

	virtual ~TestMQWorker()
	{
		m_state = WorkerState::CONSUMED;
		m_waitCV.notify_one();
		m_thread->join();
	}

	void FullfillQueue()
	{
		m_state = WorkerState::FULLFILLING;
		m_waitCV.notify_one();
	}

	void Consume()
	{
		m_state = WorkerState::CONSUMING;
		m_waitCV.notify_one();
	}

	WorkerState GetState() const { return m_state; }
private:
	bool IsNeedWakeUp()  {
		return m_state == WorkerState::CONSUMING || m_state == WorkerState::FULLFILLING || m_state == WorkerState::CONSUMED;
	}

	void WaitAndSee()
	{
		std::unique_lock<decltype(m_waitingMutex)> lock(m_waitingMutex);
		m_waitCV.wait_for(lock, std::chrono::seconds(2), std::bind(&TestMQWorker::IsNeedWakeUp, this));
	}

	void _FullfillQueue()
	{
		m_state = WorkerState::FULLFILLING;
		for (int id = 0; id < m_queuesCount; ++id)
		{
			m_keys.push_back(m_factory.getKey());
		}

		auto start = std::chrono::system_clock::now();
		for (int item = 0; item < m_queueCapacity; ++item)
		{
			for (int id = 0; id < m_queuesCount; ++id)
			{
				m_multiqueue.Enqueue(m_keys[id], std::move(m_factory.getValue(m_keys[id])));
			}
		}
		m_state = WorkerState::FULLFILLED;
		//m_chart["Queue is Fullfilled"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startFullfill);
		m_observer.OnFinishFullfilling(m_queuesCount);
	}

	void _Consume()
	{
		std::vector<std::unique_ptr<Consumer>> consumers;
		for (size_t id = 0; id < m_queuesCount; ++id)
		{
			consumers.push_back(std::move(m_factory.getConsumer(m_keys[id])));
		}

		auto startConsuming = std::chrono::system_clock::now();
		for (size_t queueID = 0; queueID < m_queuesCount; ++queueID)
		{
			IConsumer<Key, Value>* pConsumer = dynamic_cast<IConsumer<Key, Value>*>(consumers[queueID].get());
			m_multiqueue.Subscribe(m_keys[queueID], pConsumer);
		}

		for (size_t waitingQueue = 0; waitingQueue < m_queuesCount; ++waitingQueue)
		{
			if (!consumers[waitingQueue]->WaitConsuming(m_queueCapacity, std::chrono::microseconds(1000), std::chrono::seconds(59)))
			{
				std::stringstream ss;
				ss << "Items are not consumed at time. Consumed " << consumers[waitingQueue]->ConsumedCount() << ", need " << m_queueCapacity;
				throw std::runtime_error(ss.str());
			}
		}
		//m_chart["Items are consumed"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startConsuming);
		m_state = WorkerState::CONSUMED;
		m_observer.OnFinishConsuming(m_queuesCount);
	}

	void Work() 
	{
		try
		{
			while (m_state != WorkerState::CONSUMED)
			{
				if (m_state == WorkerState::FULLFILLING)
				{
					_FullfillQueue();
				}
				else if (m_state == WorkerState::CONSUMING)
				{
					_Consume();
				}
				else
				{
					WaitAndSee();
				}
			}
		}
		catch (const std::exception& ex)
		{
			cout << ex.what() << endl;
			char c;
			cin >> c;
			exit(1);
		}
	}

	size_t m_queueCapacity;
	size_t m_queuesCount;
	Factory& m_factory;
	MultiQueue& m_multiqueue;
	TestMQWorkerObserver& m_observer;
	unique_ptr<std::thread> m_thread;
	ScheduleChart m_chart;
	std::vector<Key> m_keys;
	atomic<WorkerState> m_state;
	std::condition_variable m_waitCV;
	std::mutex m_waitingMutex;

};

template<typename WorkersT>
bool AreWorkersFullfilled(WorkersT *workers)
{
	bool result = true;
	if (workers->empty())
		assert(false);

	for (auto& item = workers->begin(); item != workers->end(); ++item)
	{
		if ((item->get())->GetState() != WorkerState::FULLFILLED)
		{
			result = false;
			break;
		}
	}
	return result;
}

template<typename WorkersT>
bool AreWorkersConsumed(WorkersT *workers)
{
	bool result = true;
	if (workers->empty())
		assert(false);

	for (auto& item = workers->begin(); item != workers->end(); ++item)
	{
		if ((item->get())->GetState() != WorkerState::CONSUMED)
		{
			result = false;
			break;
		}
	}
	return result;
}

template<typename Key, typename Value, typename Consumer, typename Factory>
ScheduleChart MultiQueue_Fullfill_And_WaitTillTheEndWithConsumer(Factory& factory, size_t queuesCount,  size_t queueCapacity, size_t threadsCount)
{
	ScheduleChart chart;
	try {
		using MultiQueueProcessorT = IMultiQueueProcessor<Key, Value>;
		using TestMQWorkerT = TestMQWorker<Key, Value, Consumer, Factory, MultiQueueProcessorT>;

		assert(queuesCount >= threadsCount);
		auto start = std::chrono::system_clock::now();
		std::condition_variable waitCV;
		std::mutex waitLocker;
		TestMQWorkerObserver observer(queuesCount, queueCapacity, waitCV);

		std::unique_ptr<MultiQueueProcessorT> mq(factory.getMultiQueue());
		std::vector<std::unique_ptr<TestMQWorkerT>> workers;
		size_t queuesPerThread = queuesCount / threadsCount;
		size_t threadsWithAddQueue = queuesCount % threadsCount;

		for (size_t threadID = 0; threadID < threadsCount; ++threadID)
		{
			size_t queuesPerTheThread = queuesPerThread;
			if (threadID < threadsWithAddQueue)
				++queuesPerTheThread;

			workers.push_back(
				make_unique<TestMQWorkerT>(factory, *(mq.get()), queueCapacity, observer, queuesPerTheThread)
			);
		}

		auto startFullfill = std::chrono::system_clock::now();
		for (int threadID = 0; threadID < threadsCount; ++threadID)
		{
			workers[threadID]->FullfillQueue();
		}

		std::unique_lock<decltype(waitLocker)> waitLock(waitLocker);
		while (!AreWorkersFullfilled<decltype(workers)>(&workers))
		{
			if (!waitCV.wait_for(waitLock, std::chrono::seconds(15), std::bind(&AreWorkersFullfilled<decltype(workers)>, &workers)))
			{
				throw std::runtime_error("Fullfilling is too long");
			}
		}
		//std::bind(&AreWorkersFullfilled<decltype(workers)>, &workers)) == true);
		chart["1. Queue is Fullfilled"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startFullfill);
		waitLock.unlock();

		auto startConsuming = std::chrono::system_clock::now();
		for (int threadID = 0; threadID < threadsCount; ++threadID)
		{
			workers[threadID]->Consume();
		}

		waitLock.lock();
		while (!AreWorkersConsumed<decltype(workers)>(&workers))
		{
			if (waitCV.wait_for(waitLock, std::chrono::seconds(150), std::bind(&AreWorkersConsumed<decltype(workers)>, &workers)) == false)
			{
				throw std::runtime_error("Consumption is too long");
			}

		}

		chart["2. Items are consumed"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startConsuming);
		auto startOfStopping = std::chrono::system_clock::now();
		mq->StopProcessing();
		chart["3. MultiQueue stop"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startOfStopping);
		auto startDestroy = std::chrono::system_clock::now();
		mq.reset();
		chart["4. MultiQueue destroy"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startDestroy);
	}
	catch (const std::exception& ex)
	{
		cout << ex.what() << endl;
	}
	return chart;
}

template<typename MultiQueueProcessor, size_t items_count, size_t microseconds_consumption_time>
ScheduleChart testMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer(size_t queues_count, size_t threadsCount)
{
	auto factory = SimpleFactory<int, int, QuickConsumer<int, int, microseconds_consumption_time>, MultiQueueProcessor, ItemsGenerator<int, int>>();
	return MultiQueue_Fullfill_And_WaitTillTheEndWithConsumer<int, int, QuickConsumer<int, int, microseconds_consumption_time>, decltype(factory)>(
		factory, queues_count, items_count, threadsCount);
}

template<typename MultiQueueProcessor1, typename MultiQueueProcessor2, size_t items_count, size_t microseconds_consumption_time>
void BattleTestMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer(size_t queues_count, size_t threadsCount, const std::string& title)
{
	ScheduleChart schedule1 = testMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<MultiQueueProcessor1, items_count, microseconds_consumption_time>(queues_count, threadsCount);
	ScheduleChart schedule2 = testMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<MultiQueueProcessor2, items_count, microseconds_consumption_time>(queues_count, threadsCount);
	BattleChart result;
	std::cout << title << std::endl;
	for (auto item : schedule1)
	{
		result[item.first] = std::pair<decltype(item.second), decltype(item.second)>(schedule1[item.first], schedule2[item.first]);
		std::cout << item.first << ": " << schedule1[item.first].count() << " vs " << schedule2[item.first].count() << " ms" << std::endl;
	}
	std::cout << std::endl;
	//return result;

}

//README: Please, run and see output. It is a test of new stucture ThreadPerConsumer_MultiQueueProcessor<Key, Value>
// It was really limit time for implementation. That's why I have a lot of todo in code and comments in test below.
int main()
{
	cout << "Multithreading processing vs processing in a single for a multi queue. Let's investigate a new runtime metrics. Scenarios:" << endl;
	//cout << "New multi queue implementation. Let's investigate a new runtime metrics. Scenarios:" << endl;
	//BattleTestOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<
	//	ThreadPerConsumer_MultiQueueProcessor<int, int, 1>,
	//	OldIntMultiQueueProcT,cd ..
	//	1>
	//	("One Queue with one item processed by one consumer");


	const size_t queueCapacity1 = 1000;
	//BattleTestOneQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<
	//	ThreadPerConsumer_MultiQueueProcessor<int, int, queueCapacity>,
	//	OldIntMultiQueueProcT,
	//	queueCapacity>
	//	("One Queue with 1000 items processed by one consumer");

	cout << "1. Fullfill each queue of MultiQueue to " << queueCapacity1 << " int items." << endl;
	cout << "2. Consume all items." << endl;
	cout << "3. Stop queue." << endl;
	std::cout << std::endl;
	cout << "Let's get time of each step and play with queues count to check solution." << endl;
	std::cout << std::endl;

	const size_t microseconds_consumption_time = 0;
	for (uint8_t power = 0; power < 10; power++)
	{
		size_t queues_count = static_cast<size_t>(1llu << power);
		size_t threadsCount = (queues_count>8)?8 : queues_count;
		stringstream ss;
		ss << queues_count << " Queues:";
		BattleTestMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<
			ThreadPerConsumer_MultiQueueProcessor<int, int, queueCapacity1>,
			SingleThread_MultiQueueProcessor<int, int, queueCapacity1>,
			queueCapacity1, 
			microseconds_consumption_time
		>(queues_count, threadsCount, ss.str());
	}
	cout << "We can see that single thread queue processor is not so bad" << endl;

	cout << "We can see that single thread queue processor is not so bad" << endl;

	const size_t queueCapacity2 = 10000;
	cout << "But lets try to enlarge queue size to check more long jobs. Queue size" << queueCapacity2 << " and repeat test." << endl;
	for (uint8_t power = 0; power < 8; power++)
	{
		size_t queues_count = static_cast<size_t>(1llu << power);
		size_t threadsCount = (queues_count > 8) ? 8 : queues_count;
		stringstream ss;
		ss << queues_count << " Queues:";
		BattleTestMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<
			ThreadPerConsumer_MultiQueueProcessor<int, int, queueCapacity2>,
			SingleThread_MultiQueueProcessor<int, int, queueCapacity2>,
			queueCapacity2,
			microseconds_consumption_time
		>(queues_count, threadsCount, ss.str());
	}
	cout << "Looks like Multi thread queue processing shows better results." << endl;
	const size_t microseconds_consumption_time1 = 1000;// My computer can't wait  less 1.1 ms
	cout << "Lets return a queue size back and add delay for a " << microseconds_consumption_time1/1000 << " ms to consumer and repeat the first tests suit." << endl;
	for (uint8_t power = 0; power < 5; power++)
	{
		size_t queues_count = static_cast<size_t>(1llu << power);
		size_t threadsCount = (queues_count > 8) ? 8 : queues_count;
		stringstream ss;
		ss << queues_count << " Queues:";
		BattleTestMultiQueue_Fullfill_And_WaitTillTheEndWithOneFastConsumer<
			ThreadPerConsumer_MultiQueueProcessor<int, int, queueCapacity1>,
			SingleThread_MultiQueueProcessor<int, int, queueCapacity1>,
			queueCapacity1,
			microseconds_consumption_time1
		>(queues_count, threadsCount, ss.str());
	}
	cout << "Looks like Multi thread queue processing is better again." << endl;

	char i;
	cin >> i;
	return 0;
}
