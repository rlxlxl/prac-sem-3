#pragma once
#include <vector>
#include <optional>
#include <functional>
#include <string>

template<typename K, typename V>
class HashMap {
private:
    struct Node {
        K key;
        V value;
        bool occupied = false;
    };

    std::vector<Node> data;
    size_t count = 0;
    double load_factor;

    size_t hash(const K& key) const {
        return std::hash<K>{}(key) % data.size();
    }

public:
    HashMap(size_t initial_size = 32, double lf = 0.75)
        : data(initial_size), load_factor(lf) {}

    void put(const K& key, const V& value) {
        size_t idx = hash(key);

        while (data[idx].occupied && data[idx].key != key) {
            idx = (idx + 1) % data.size();
        }

        if (!data[idx].occupied) {
            data[idx].occupied = true;
            data[idx].key = key;
            count++;
        }

        data[idx].value = value;
    }

    std::optional<V> get(const K& key) const {
        size_t idx = hash(key);

        while (data[idx].occupied) {
            if (data[idx].key == key)
                return data[idx].value;

            idx = (idx + 1) % data.size();
        }
        return std::nullopt;
    }

    void remove(const K& key) {
        size_t idx = hash(key);

        while (data[idx].occupied) {
            if (data[idx].key == key) {
                data[idx].occupied = false;
                return;
            }
            idx = (idx + 1) % data.size();
        }
    }

    std::vector<std::pair<K,V>> items() const {
        std::vector<std::pair<K,V>> out;
        out.reserve(count);

        for (auto &n : data)
            if (n.occupied)
                out.emplace_back(n.key, n.value);

        return out;
    }
};