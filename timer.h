#pragma once
#include "time.h"
#include "set.h"
#include "function.h"

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
	timestamp trigger_time;
	timestamp pause_hold;
	timestamp period;
	function<timer_trigger_postaction()> callback;

	timer();
	~timer();
	void run();
	void pause();
	void reset();
	timestamp remaining_time();
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
		return key_value->trigger_time < value.key_value->trigger_time
			|| key_value->trigger_time == value.key_value->trigger_time
			&& key_value < value.key_value;
	}
};

set<timer *> *timers();
