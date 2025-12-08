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

    std::vector<Node> data_;
    size_t count_ = 0;
    double loadFactor_;

    // Вычисление индекса хеш-значения
    size_t hashKey(const K& key) const {
        return std::hash<K>{}(key) % data_.size();
    }

public:
    HashMap(size_t initialSize = 32, double loadFactor = 0.75)
        : data_(initialSize), loadFactor_(loadFactor) {}

    // Вставка или обновление элемента
    void put(const K& key, const V& value) {
        size_t idx = hashKey(key);

        while (data_[idx].occupied && data_[idx].key != key) {
            idx = (idx + 1) % data_.size();
        }

        if (!data_[idx].occupied) {
            data_[idx].occupied = true;
            data_[idx].key = key;
            count_++;
        }

        data_[idx].value = value;
    }

    // Получение элемента
    std::optional<V> get(const K& key) const {
        size_t idx = hashKey(key);

        while (data_[idx].occupied) {
            if (data_[idx].key == key)
                return data_[idx].value;

            idx = (idx + 1) % data_.size();
        }

        return std::nullopt;
    }

    // Удаление элемента
    void remove(const K& key) {
        size_t idx = hashKey(key);

        while (data_[idx].occupied) {
            if (data_[idx].key == key) {
                data_[idx].occupied = false;
                return;
            }
            idx = (idx + 1) % data_.size();
        }
    }

    // Получение всех элементов в виде вектора пар
    std::vector<std::pair<K, V>> items() const {
        std::vector<std::pair<K, V>> out;
        out.reserve(count_);

        for (const auto& node : data_) {
            if (node.occupied) {
                out.emplace_back(node.key, node.value);
            }
        }

        return out;
    }

    // Дополнительно можно добавить метод size()
    size_t size() const {
        return count_;
    }
};
