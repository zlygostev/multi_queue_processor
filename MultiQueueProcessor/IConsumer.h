#pragma once

template<typename Key, typename Value>
struct IConsumer
{
	// It is a question to interface is to nothrow?
	virtual void Consume(Key id, const Value &value) = 0;
};