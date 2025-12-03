#include "db.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

Database::Database(const std::string& dir)
    : dir_path(dir)
{
    if (!fs::exists(dir_path)) {
        fs::create_directories(dir_path);
    }
}

std::shared_ptr<Collection> Database::open_collection(const std::string& name) {
    std::string path = dir_path + "/" + name + ".json";
    return std::make_shared<Collection>(path);
}