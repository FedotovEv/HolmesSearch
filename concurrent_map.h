#pragma once
#include <cstdlib>
#include <map>
#include <vector>
#include <mutex>

using namespace std;

template <typename Key, typename Value>
class ConcurrentMap
{
public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap поддерживает только целочисленные ключи");

	struct Access
	{
		Access(Value& value, mutex& access_mutex) : refv(value), ref_to_mutex(access_mutex)
		{}

		~Access()
		{
			ref_to_mutex.unlock();
		}

		Value& refv;
		mutex& ref_to_mutex;
	};

	explicit ConcurrentMap(size_t bucket_count) : map_vector(bucket_count), mutex_vector(bucket_count)
	{}

	Access operator[](const Key& key)
	{
		size_t map_num = GetMapNum(key);
		mutex_vector[map_num].lock();
		return {map_vector[map_num][key], mutex_vector[map_num]};
	}

	Access at(const Key& key)
	{
		size_t map_num = GetMapNum(key);
		mutex_vector[map_num].lock();
		return {map_vector[map_num].at(key), mutex_vector[map_num]};
	}

	size_t count(const Key& key)
	{
		size_t map_num = GetMapNum(key);
		lock_guard lg(mutex_vector[map_num]);
		return map_vector[map_num].count(key);
	}

	size_t erase(const Key& key)
	{
		size_t map_num = GetMapNum(key);
		lock_guard lg(mutex_vector[map_num]);
		return map_vector[map_num].erase(key);
	}

    map<Key, Value> BuildOrdinaryMap()
	{
		map<Key, Value> result;
		for (size_t i = 0; i < map_vector.size(); ++i)
		{
			lock_guard lg(mutex_vector[i]);
			for (auto& cur_element_pair : map_vector[i])
				result[cur_element_pair.first] = cur_element_pair.second;
		}
		return result;
	}

private:
	vector<map<Key, Value>> map_vector;
	vector<mutex> mutex_vector;

	size_t GetMapNum(const Key& key)
	{
	    return (key - numeric_limits<Key>::min()) % map_vector.size();
	}
};
