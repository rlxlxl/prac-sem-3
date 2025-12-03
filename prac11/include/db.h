#pragma once
#include "collection.h"
#include "json.hpp"
#include <string>
#include <memory>

using json = nlohmann::json;

class Database {
public:
    Database(const std::string& dir);
    std::shared_ptr<Collection> open_collection(const std::string& name);

private:
    std::string dir_path;
};