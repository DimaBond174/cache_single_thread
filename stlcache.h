#ifndef STLCACHE_H
#define STLCACHE_H
// thnks to https://jaeyu.wordpress.com/2014/04/15/lru-cache-in-c/
#include <list>
#include <map>
#include <cassert>

template<typename K, typename V>
class LRU {
  public:
    explicit LRU(int capacity) : capacity_(capacity) { }

    ~LRU() {
      kv_map_.clear();
      lru_tracker_.clear();
    }

    /**
     * @brief Get value associated with a key
     * @param key Key
     * @return Reference to the value associated with the key
     */
    V& get(K key) {
      assert(kv_map_.find(key) != kv_map_.end());
      promote(key);
      return kv_map_[key].first;
    }

    /**
     * @brief Check if the key exists in a cache.
     * @param key     Key
     * @return false  if key does not exiist
     * @return true   if key exiists
     */
    bool exist(K key) {
      if (kv_map_.find(key) == kv_map_.end()) {
        return false;
      }
      return true;
    }

    /**
     * @brief   Add KV pair into cache
     * @param   key Key
     * @param   val Value
     */
    void put(K key, V val) {
      if (kv_map_.find(key) != kv_map_.end()) {
        kv_map_[key].first = val;
      } else {
        insert(key, val);
      }
      promote(key);
    }

  private:
    typedef std::list<K> lru_tracker_t;
    typedef std::map<K, std::pair<V,
              typename lru_tracker_t::iterator> > kv_map_t;

    /* The maximum number of items to be maintained in a cache. */
    int capacity_;

    /* Map between key and value */
    kv_map_t kv_map_;

    /* keep the least recently used item at the head, and
     * the most recently used item at the tail of the list.
     */
    lru_tracker_t lru_tracker_;

    /**
     * @brief Insert a KV pair into KV map and update the LRU tracker.
     *
     * @param key Key
     * @param val Value
     */
    void insert(K &key, V &val) {
      if (capacity_ == 0) {
        evict();
      }
      typename lru_tracker_t::iterator it =
          lru_tracker_.insert(lru_tracker_.end(), key);
      kv_map_[key] = std::make_pair(val, it);
      capacity_--;
    }

    /**
     * @brief Evict the least recently used KV pair from the cache.
     */
    void evict() {
      typename kv_map_t::iterator it =
          kv_map_.find(lru_tracker_.front());
      kv_map_.erase(it);
      lru_tracker_.pop_front();
      capacity_++;
    }

    /**
     * @brief Make the key and associated value to be the most recently
     *        used one by moving the pair to the end of the list.
     *
     * @param key Key to be promoted
     */
    void promote(K &key) {
      if (lru_tracker_.size() > 0) {
        lru_tracker_.splice(lru_tracker_.end(),
            lru_tracker_,
            kv_map_[key].second);
      }
    }
};
#endif // STLCACHE_H
