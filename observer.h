#pragma once
#include "array.h"
#include "function.h"

template<typename ...args> struct observer
{
	array<function<void(args...)>> callbacks;

	void trigger(args... arguments)
	{
		for(uint64 i = 0; i < callbacks.size; i++)
		{
			callbacks[i](arguments...);
		}
	}
};
