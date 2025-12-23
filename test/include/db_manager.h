#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "database.h"
#include <mutex>
#include <map>
#include <memory>
#include <shared_mutex>

class DatabaseManager {
private:
    // Блокировки для каждой базы данных
    std::map<std::string, std::shared_ptr<std::shared_mutex>> dbLocks_;
    std::mutex locksMutex_; // Защищает сам map блокировок
    
    // Получить или создать блокировку для базы данных
    std::shared_ptr<std::shared_mutex> getLock(const std::string& dbName) {
        std::lock_guard<std::mutex> lock(locksMutex_);
        if (dbLocks_.find(dbName) == dbLocks_.end()) {
            dbLocks_[dbName] = std::make_shared<std::shared_mutex>();
        }
        return dbLocks_[dbName];
    }
    
public:
    // Выполнить операцию с блокировкой на чтение
    template<typename Func>
    auto executeRead(const std::string& dbName, const std::string& collectionName, Func func) {
        auto lock = getLock(dbName);
        std::shared_lock<std::shared_mutex> sharedLock(*lock);
        Database db(dbName, collectionName);
        return func(db);
    }
    
    // Выполнить операцию с блокировкой на запись
    template<typename Func>
    auto executeWrite(const std::string& dbName, const std::string& collectionName, Func func) {
        auto lock = getLock(dbName);
        std::unique_lock<std::shared_mutex> uniqueLock(*lock);
        Database db(dbName, collectionName);
        return func(db);
    }
};

#endif // DB_MANAGER_H

