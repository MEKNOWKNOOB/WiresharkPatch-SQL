#ifndef PSQL_DATABASE_HELPER_H
#define PSQL_DATABASE_HELPER_H

#include <iostream>
#include <string>
#include <vector>
#include <tuple>

class PSQLDatabaseHelper
{
public:
    PSQLDatabaseHelper(std::string tableName, std::vector<std::tuple<std::string, std::string>> columns);
    ~PSQLDatabaseHelper();

    std::string CreateInitializeTableQuery();
    std::string CreateAddPacketQuery(std::vector<std::vector<std::string>> data);

private:
    std::string tableName;
    std::vector<std::tuple<std::string, std::string>> columns;
};

#endif