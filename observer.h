#pragma once
#include "array.h"

template<typename ...args> struct observer
{
	array<void(*)(args...)> callbacks;

	void trigger(args... arguments)
	{
		for(uint64 i = 0; i < callbacks.size; i++)
		{
			callbacks[i](arguments...);
		}
	}
};
