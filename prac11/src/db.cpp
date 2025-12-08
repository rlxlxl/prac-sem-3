#include "db.h"
#include <filesystem>
#include <iostream>
using namespace std;
namespace fs = filesystem;

Database::Database(const string& directoryPath)
    : dirPath_(directoryPath)
{
    if (!fs::exists(dirPath_)) {
        fs::create_directories(dirPath_);
    }
}

shared_ptr<Collection> Database::openCollection(const string& name) {
    string path = dirPath_ + "/" + name + ".json";
    return make_shared<Collection>(path);
}
