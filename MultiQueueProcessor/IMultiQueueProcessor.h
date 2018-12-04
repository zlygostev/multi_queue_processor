#pragma once
#include "IConsumer.h"

//TODO. Need to ask consumers to change interface from parameters by value to parameters by links
//I need to refactor implementation of this interface. I can't change interface.
template<typename Key, typename Value>
struct IMultiQueueProcessor
{
	virtual ~IMultiQueueProcessor() = default;

	virtual void Subscribe(Key id, IConsumer<Key, Value> * consumer) = 0;

	virtual void Unsubscribe(Key id) = 0;

	virtual void Enqueue(Key id, Value value) = 0;
	//should throw QueueIsEmpty if no more items in queue
	virtual Value Dequeue(Key id) = 0;
	virtual void StopProcessing() = 0;
};
