#pragma once
#include "collection.h"
#include "json.hpp"
#include <string>
#include <memory>
using namespace std;
using json = nlohmann::json;

class Database {
public:
    Database(const string& directoryPath);

    shared_ptr<Collection> openCollection(const string& name);

private:
    string dirPath_;  
};
