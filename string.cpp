#include "string.h"

string_mapping_config smp;

string_mapping_config::string_mapping_config()
{
	min_fraction_digits = 1;
	max_fraction_digits = 9;
}

string_mapping_config* string_config()
{
	return &smp;
}
