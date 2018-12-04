#pragma once

struct QueueIsEmpty: std::logic_error
{
	QueueIsEmpty() :std::logic_error("The queue is empty. No more items.") {}
	QueueIsEmpty(const std::string& ex):std::logic_error(ex) {}
};



template<typename Value, size_t MaxQueueCapacity>
struct IQueue
{

	IQueue() :
		m_maxCapacity(MaxQueueCapacity)
	{}
	virtual ~IQueue() = default;

	//Should throw std::overflow_error if max capacity of queue is reached
	virtual void Enqueue(Value&& value) = 0;

	//Should throw QueueIsEmpty if no more entries
	virtual Value Dequeue() = 0;

	virtual bool IsEmpty() = 0;
protected:
	const size_t m_maxCapacity;
};

//Interface of queue with ability of notifications by new arrivals
//Please run m_notifySubscriber() on successful Enqueue 
template<typename Value, size_t MaxQueueCapacity>
struct IQueueWithNotifications: IQueue<Value, MaxQueueCapacity>
{
	//Notification about a new item income. Please implement it as threadsafe outside if the queue is a multithreading
	using NotifySubscriberT = std::function<void()>;

	IQueueWithNotifications(NotifySubscriberT notifySubscriber) : IQueue(),
		m_notifySubscriber(notifySubscriber)
	{}
	virtual ~IQueueWithNotifications() = default;
protected:
	NotifySubscriberT m_notifySubscriber;
};