#pragma once
//Multithread queue interface
template<typename Value, size_t MaxQueueCapacity>
struct IQueue
{
	// Notification about a new item income. 
	using NotifySubscriberT = std::function<void()>;

	//struct TimeOutException : std::exception {};

	IQueue(NotifySubscriberT notifySubscriber) :
		m_notifySubscriber(notifySubscriber),
		m_maxCapacity(MaxQueueCapacity)
	{}
	virtual ~IQueue() = default;

	virtual void Enqueue(Value&& value) = 0;
	//Return false if there is no value
	virtual bool Dequeue(Value& value) = 0;

	virtual bool IsEmpty() = 0;
	//virtual Value Dequeue(std::chrono::milliseconds timeout) = nullptr; //throw timeout exception. //TODO. Solve if it is really need
	//virtual void Stop(const Key& key) = nullptr;//TODO. Solve if it is really need
protected:
	NotifySubscriberT m_notifySubscriber;
	const size_t m_maxCapacity;
};