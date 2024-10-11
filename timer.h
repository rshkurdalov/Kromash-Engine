#pragma once
#include "time.h"
#include "set.h"

enum struct timer_state
{
	inactive,
	active,
	paused
};

enum struct timer_trigger_postaction
{
	/*Once more repeat timer action starting from last trigger_time*/
	repeat,
	/*Repeat timer action without calculating activation delta error*/
	reactivate,
	/*Dont activate timer*/
	terminate
};

struct timer
{
	timer_state state;
	nanoseconds trigger_time;
	nanoseconds pause_hold;
	nanoseconds period;
	timer_trigger_postaction (*callback)(void *data);
	void *data;

	timer();
	~timer();
	void run();
	void pause();
	void reset();
	nanoseconds remaining_time();
};

template<> struct key<timer *>
{
	timer *key_value;

	key(timer *timer_addr)
	{
		key_value = timer_addr;
	}

	bool operator<(const key &value) const
	{
		return key_value->trigger_time.value < value.key_value->trigger_time.value
			|| key_value->trigger_time.value == value.key_value->trigger_time.value
			&& key_value < value.key_value;
	}
};

set<timer *> *timers();
