#pragma once
// Have not done yet
//TODO : Do it 
//#include <map>
////#include <list>
////
////#include <thread>
//#include <mutex>
////#include <condition_variable>
//#include <atomic>
////#include <chrono>
//#include "IConsumer.h"
//#include "IQueue.h"
//#include "IQueueProcessor.h"
//#include "IMultiQueueProcessor.h"
//#include "MultiQueueProcessorFactory.h"
//
//
//
//
////#define MaxCapacity 1000
//const static size_t  MaxQueueCapacity = 1000;
//
//template<typename Key>
//struct INewItemsInQueueObserver
//{
//	virtual void HandleNewItemsEvent(const Key&) = 0
//};
//
//template<typename Key>
//struct NewItemsInQueueSubscriber
//{
//	using Observer = INewItemsInQueueObserver<Key>;
//	NewItemsInQueueSubscriber(Key key, Observer* observer) : key(std::move(key)), observer(observer)
//	{}
//
//	void operator()()
//	{
//		if (observer)
//			observer->HandleNewItemsEvent(key);
//	}
//private:
//	Key key;
//	Observer* observer;
//};
//
//template<typename Key, typename Value, int WorkersCount = 16, int MaxCapacity = MaxQueueCapacity, typename ItemsFactory = MultiQueueProcessorFactory<Key, Value>>
//struct FixedThreadsMultiQueueProcessor : IMultiQueueProcessor<Key, Value>, INewItemsInQueueObserver<Key>
//{
//	struct QueueNode
//	{
//		QueueNode() {}
//		std::mutex locker;
//		std::unique_ptr<IQueue> queue;
//	};
//
//	struct QueueWorkNode
//	{
//		QueueNode() :counter(0) {}
//		std::mutex locker;
//		std::unique_ptr<Consumer> consumer;
//		std::atomic<size_t> counter;
//	};
//
//
//	FixedThreadsMultiQueueProcessor() {}
//
//	virtual ~FixedThreadsMultiQueueProcessor()
//	{
//		StopProcessing();
//		//Stop all workers;
//		//Join them
//	};
//
//	void StopProcessing()
//	{
//		running = false;
//	}
//
//	void Subscribe(Key id, IConsumer<Key, Value> * consumer)
//	{
//		if (!running)
//		{
//			return;
//		}
//		std::unique_lock<decltype(m_multiQueueWorkMutex)> multiQueueWorkerLock(m_multiQueueMutex);
//		auto iter = m_multiQueueWork.find(id);
//		(iter == m_multiQueueWork.end())
//		{
//			auto insertedState = m_multiQueueWork.insert(std::make_pair(id, QueueWorkNode()));
//			iter = insertedState.first;
//		}
//		if (!iter->consumer)
//		{
//			iter->consumer = std::move(ItemsFactory::CreateQueue(NewItemsInQueueSubscriber<Key>(id, this), MaxCapacity));
//		}
//		multiQueueWorkerLock.unlock();
//		std::unique_lock<decltype(iter->locker)> queueLock(iter->locker);
//		iter->queue->Enqueue(std::move(value));
//
//	}
//
//	void Unsubscribe(Key id)
//	{
//		//Wait end of proccessing of consumer synchronizely, but it is better to stop processing
//		if (!running)
//		{
//			throw std::runtime_error("MultiQueueProcessor is stopping. Message stop to come soon");// Possible it's not good idea at all. We should wait synchronely of events end
//		}
//	};
//
//	void HandleNewItemsInQueueEvent(const Key&) = 0;
//
//	void Enqueue(Key id, Value value)
//	{
//		if (!running)
//		{
//			throw std::runtime_error("MultiQueueProcessor is stopped");
//		}
//		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
//		auto iter = m_multiQueue.find(id);
//		(iter == m_multiQueue.end())
//		{
//			auto insertedState = m_multiQueue.insert(std::make_pair(id, QueueNode()));
//			iter = insertedState.first;
//		}
//		if (!iter->queue)
//		{
//			iter->queue = std::move(ItemsFactory::CreateQueue(NewItemsInQueueSubscriber<Key>(id, this), MaxCapacity));
//		}
//		multiQueueLock.unlock();
//		std::unique_lock<decltype(iter->locker)> queueLock(iter->locker);
//		iter->queue->Enqueue(std::move(value));
//	}
//
//	Value Dequeue(Key id)
//	{
//		if (!running)
//		{
//			throw std::runtime_error("MultiQueueProcessor is stopped");
//		}
//		std::unique_lock<decltype(m_multiQueueMutex)> multiQueueLock(m_multiQueueMutex);
//		auto iter = m_multiQueue.find(id);
//		(iter == m_multiQueue.end())
//		{
//			throw std::invalid_argument("There is no queue for requested key");
//		}
//		if (!iter->queue)
//		{
//			throw std::out_of_range("There is no one item in the queue ever.");
//		}
//		multiQueueLock.unlock();
//		std::unique_lock<decltype(iter->mutex)> queueLock(iter->mutex);
//		Value val;
//		if (!iter->queue->Dequeue(val))
//		{
//			throw std::out_of_range("The queue is empty");
//		}
//		return val;
//	}
//
//private:
//	atomic<bool> running;
//	std::mutex m_multiQueueMutex;//Lock of multi queue level
//	std::map<Key, QueueNode> m_multiQueue;
//
//	std::mutex m_multiQueueWorkMutex;//Lock of multi queue work level
//	std::map<Key, QueueWorkNode> m_multiQueueWork;
//};
//
