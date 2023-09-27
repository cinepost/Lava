#ifndef LAVA_UTILS_UT_LRUCACHE_H_
#define LAVA_UTILS_UT_LRUCACHE_H_

#include <list>
#include <unordered_map>
#include <memory>

namespace lava { namespace ut { namespace data {

template<typename key_t, typename value_t>
class LRUCache {
	public:
		typedef typename std::pair<key_t, value_t> key_value_pair_t;
		typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

		using UniquePtr = std::unique_ptr<lava::ut::data::LRUCache<key_t, value_t>>;

		LRUCache(size_t max_elements_count) :
			mMaxCacheElementsCount(max_elements_count) {
		};

		static UniquePtr create(size_t max_elements_count) {
			return std::make_unique<LRUCache>(max_elements_count);
		};

		void put(const key_t& key, const value_t& value) {
			auto it = mCacheItemsMap.find(key);
			mCacheItemsList.push_front(key_value_pair_t(key, value));
			if (it != mCacheItemsMap.end()) {
				mCacheItemsList.erase(it->second);
				mCacheItemsMap.erase(it);
			}
			mCacheItemsMap[key] = mCacheItemsList.begin();
			
			if (mCacheItemsMap.size() > mMaxCacheElementsCount) {
				auto last = mCacheItemsList.end();
				last--;
				mCacheItemsMap.erase(last->first);
				mCacheItemsList.pop_back();
			}
		}

		bool get(const key_t& key, value_t& value) {
			
		}
		
		const value_t& get(const key_t& key) {
			auto it = mCacheItemsMap.find(key);
			if (it == mCacheItemsMap.end()) {
				throw std::range_error("There is no such key in cache");
			} else {
				mCacheItemsList.splice(mCacheItemsList.begin(), mCacheItemsList, it->second);
				return it->second->second;
			}
		}
		
		bool exists(const key_t& key) const {
			return mCacheItemsMap.find(key) != mCacheItemsMap.end();
		}
		
		size_t size() const {
			return mCacheItemsMap.size();
		}
		
	private:
		std::list<key_value_pair_t> mCacheItemsList;
		std::unordered_map<key_t, list_iterator_t> mCacheItemsMap;
		size_t mMaxCacheElementsCount;
};

}}} // namespace lava::ut::data

#endif	// LAVA_UTILS_UT_LRUCACHE_H_