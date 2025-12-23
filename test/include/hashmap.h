#ifndef HASHMAP_H
#define HASHMAP_H

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <cstddef>
#include <type_traits>
#include <functional>

template<typename K, typename V>
class HashMap {
private:
    struct Node {
        K key;
        V value;
        Node* next;
        
        Node(const K& k, const V& v) : key(k), value(v), next(nullptr) {}
    };
    
    std::vector<Node*> buckets;
    size_t size_;
    size_t capacity_;
    static constexpr double LOAD_FACTOR = 0.75;
    static constexpr size_t INITIAL_CAPACITY = 16;
    
    // Собственная хэш-функция для строк
    size_t hash(const std::string& key) const {
        size_t hash_value = 5381;
        for (char c : key) {
            hash_value = ((hash_value << 5) + hash_value) + static_cast<unsigned char>(c);
        }
        return hash_value;
    }
    
    size_t getBucketIndex(const K& key) const {
        if constexpr (std::is_same_v<K, std::string>) {
            return hash(key) % capacity_;
        } else {
            return std::hash<K>{}(key) % capacity_;
        }
    }
    
    void resize() {
        size_t old_capacity = capacity_;
        capacity_ *= 2;
        std::vector<Node*> new_buckets(capacity_, nullptr);
        
        for (size_t i = 0; i < old_capacity; ++i) {
            Node* current = buckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                size_t new_index = getBucketIndex(current->key);
                current->next = new_buckets[new_index];
                new_buckets[new_index] = current;
                current = next;
            }
        }
        
        buckets = std::move(new_buckets);
    }
    
public:
    HashMap() : size_(0), capacity_(INITIAL_CAPACITY) {
        buckets.resize(capacity_, nullptr);
    }
    
    ~HashMap() {
        clear();
    }
    
    HashMap(const HashMap& other) : size_(0), capacity_(other.capacity_) {
        buckets.resize(capacity_, nullptr);
        for (const auto& pair : other.items()) {
            put(pair.first, pair.second);
        }
    }
    
    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            clear();
            capacity_ = other.capacity_;
            buckets.resize(capacity_, nullptr);
            for (const auto& pair : other.items()) {
                put(pair.first, pair.second);
            }
        }
        return *this;
    }
    
    void put(const K& key, const V& value) {
        size_t index = getBucketIndex(key);
        Node* current = buckets[index];
        
        // Проверяем, существует ли уже ключ
        while (current != nullptr) {
            if (current->key == key) {
                current->value = value;
                return;
            }
            current = current->next;
        }
        
        // Добавляем новый узел в начало цепочки
        Node* new_node = new Node(key, value);
        new_node->next = buckets[index];
        buckets[index] = new_node;
        size_++;
        
        // Проверяем необходимость расширения
        if (static_cast<double>(size_) / capacity_ >= LOAD_FACTOR) {
            resize();
        }
    }
    
    bool get(const K& key, V& value) const {
        size_t index = getBucketIndex(key);
        Node* current = buckets[index];
        
        while (current != nullptr) {
            if (current->key == key) {
                value = current->value;
                return true;
            }
            current = current->next;
        }
        
        return false;
    }
    
    bool contains(const K& key) const {
        size_t index = getBucketIndex(key);
        Node* current = buckets[index];
        
        while (current != nullptr) {
            if (current->key == key) {
                return true;
            }
            current = current->next;
        }
        
        return false;
    }
    
    bool remove(const K& key) {
        size_t index = getBucketIndex(key);
        Node* current = buckets[index];
        Node* prev = nullptr;
        
        while (current != nullptr) {
            if (current->key == key) {
                if (prev == nullptr) {
                    buckets[index] = current->next;
                } else {
                    prev->next = current->next;
                }
                delete current;
                size_--;
                return true;
            }
            prev = current;
            current = current->next;
        }
        
        return false;
    }
    
    std::vector<std::pair<K, V>> items() const {
        std::vector<std::pair<K, V>> result;
        for (size_t i = 0; i < capacity_; ++i) {
            Node* current = buckets[i];
            while (current != nullptr) {
                result.push_back({current->key, current->value});
                current = current->next;
            }
        }
        return result;
    }
    
    size_t size() const {
        return size_;
    }
    
    bool empty() const {
        return size_ == 0;
    }
    
    void clear() {
        for (size_t i = 0; i < capacity_; ++i) {
            Node* current = buckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                delete current;
                current = next;
            }
            buckets[i] = nullptr;
        }
        size_ = 0;
    }
};

#endif // HASHMAP_H

