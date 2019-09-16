#ifndef LAZY_HEAP
#define LAZY_HEAP

#include <map>
#include <queue>
#include <stdexcept>
#include <vector>

template<
    typename T,
    typename CompareHeap,
    typename CompareHash
    >
class LazyHeap {
public:
    LazyHeap() = default;
    LazyHeap(LazyHeap const&) = default;
    LazyHeap(LazyHeap&&) = default;
    LazyHeap& operator=(LazyHeap const&) = default;
    LazyHeap& operator=(LazyHeap&&) = default;

    struct ExpirableKey {
        T key;
        int t;

        ExpirableKey(T const& _key, int _t): key(_key), t(_t) {}
    };

    struct CompareExpirableKey {
        bool operator()(ExpirableKey const& a, ExpirableKey const& b) const {
            return CompareHeap{}(a.key, b.key);
        }
    };

    T const& top() const {
        return m_heap.top().key;
    }

    void pop() {
        if (m_heap.empty())
            throw std::runtime_error("Empty LazyHeap");
        m_heap.pop();
        clear_outdated();
    }

    void update_key(T const& key) {
        auto it_key = m_last_update.find(key);
        int new_t = (it_key == end(m_last_update))? 0 : it_key->second+1;
        m_heap.emplace(key, new_t);
        m_last_update[key] = new_t;
        clear_outdated();
    }

    bool empty() const {
        return m_heap.empty();
    }

    double overload_ratio() const {
      if (m_last_update.empty())
        return 1.;
      return m_heap.size() / m_last_update.size();
    }

private:
    std::priority_queue<
        ExpirableKey,
        std::vector<ExpirableKey>,
        CompareExpirableKey
        > m_heap;
    std::map<T, int, CompareHash> m_last_update;

    void clear_outdated() {
        auto elem = m_heap.top();
        auto it_key = m_last_update.find(elem.key);
        int last_t = it_key->second;
        while (elem.t < last_t) {
            m_heap.pop();
            if (m_heap.empty())
                break ;
            elem = m_heap.top();
            it_key = m_last_update.find(elem.key);
            last_t = it_key->second;
        }
    }
};

template<
    typename T,
    typename CompareHeap,
    typename CompareHash
    >
void save_memory(LazyHeap<T, CompareHeap, CompareHash>& heap) {
  LazyHeap<T, CompareHeap, CompareHash> other;
  while (!heap.empty()) {
    auto elem = heap.top();
    heap.pop();
    other.update_key(elem);
  }
  heap = other;
}

#endif
