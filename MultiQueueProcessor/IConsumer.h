#pragma once

template<typename Key, typename Value>
struct IConsumer
{
	virtual void Consume(Key id, const Value &value) = 0;
};