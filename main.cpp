#include "array.h"
#include "tests.h"
#include "vector.h"
#include "matrix.h"
#include "os_api.h"
#include <Windows.h>

#include <map>
template<typename K, typename V>
class interval_map {
	V m_valBegin;
	std::map<K,V> m_map;
public:
	// constructor associates whole range of K with val
	template<typename V_forward>
	interval_map(V_forward&& val)
	: m_valBegin(std::forward<V_forward>(val)) {}

	template<typename V_forward>
	void assign( K const& keyBegin, K const& keyEnd, V_forward&& val )
	{
		if(!(keyBegin < keyEnd)) return;
		V last = (*this)[keyEnd];
		auto it = m_map.upper_bound(keyBegin);
		if(it == m_map.begin())
		{
			if(!(val == m_valBegin))
			{
				it = m_map.insert(std::pair<K, V>(keyBegin, std::forward<V_forward>(val))).first;
				it++;
			}
		}
		else
		{
			it--;
			if(!(it->second == val))
			{
				if(!(it->first < keyBegin))
					it = m_map.erase(it);
				if(!((*this)[keyBegin] == val))
				{
					it = m_map.insert(std::pair<K, V>(keyBegin, std::forward<V_forward>(val))).first;
					it++;
				}
			}
			else it++;
		}
		auto it_end = m_map.lower_bound(keyEnd);
		while(it != it_end)
			it = m_map.erase(it);
		it = m_map.find(keyEnd);
		if(it != m_map.end())
		{
			if(it == m_map.begin())
			{
				if(it->second == m_valBegin)
					m_map.erase(it);
			}
			else
			{
				auto prev = it;
				prev--;
				if(prev->second == it->second)
					m_map.erase(it);
			}
		}
		if(m_map.find(keyEnd) == m_map.end() && !(val == last))
			m_map.insert(std::pair<K, V>(keyEnd, last));
	}

	// look-up of the value associated with key
	V const& operator[]( K const& key ) const {
		auto it=m_map.upper_bound(key);
		if(it==m_map.begin()) {
			return m_valBegin;
		} else {
			return (--it)->second;
		}
	}
};

int __stdcall wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR lpCmdLine,
	int nCmdShow)
{
	//test_array();
	//test_string();
	//test_vector();
	//test_matrix();
	//test_real();
	//test_linear_algebra();
	test_ui();
	//test_set();
	//test_file();
	//test_fileset();
	//test_web_server();
	//test_json();
	//test_database();

	os_message_loop();

	return 0;
}
