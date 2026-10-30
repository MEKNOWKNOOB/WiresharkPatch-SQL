#include "psql_database_helper.h"


PSQLDatabaseHelper::PSQLDatabaseHelper(std::string tableName, std::vector<std::tuple<std::string, std::string>> columns)
{
    this->tableName = tableName;
    this->columns = columns; // Vector of (type, name)
}

PSQLDatabaseHelper::~PSQLDatabaseHelper()
{

}

std::string PSQLDatabaseHelper::CreateInitializeTableQuery()
{
    // Initialize the table, given the name and columns
    std::cout << "Starting CreateInitializeTableQuery" << std::endl;

    std::string query = "";

    query += "CREATE TABLE ";

    // Put the table name
    query += tableName + "(\n";
    for (int i = 0; i < columns.size(); i++)
    {
        std::cout << i << std::endl;
        query += std::get<1>(columns[i]) + " " + std::get<0>(columns[i]);
        if (i != columns.size() - 1)
        {
            query += ",";
        }
        query += "\n";
    }
    query += ");";
    std::cout << "After for loop" << std::endl;

    // Put in all of the variables
    // CREATE TABLE cars (
    // brand VARCHAR(255),
    // model VARCHAR(255),
    // year INT
    // );"

    return query;
}

std::string PSQLDatabaseHelper::CreateAddPacketQuery(std::vector<std::vector<std::string>> data)
{
    // Construct a query to push the packet data to the table
    // Query construction
    std::string query = "";
    
    /*
    INSERT INTO cars (brand, model, year)
    VALUES
    ('Volvo', 'p1800', 1968),
    ('BMW', 'M1', 1978),
    ('Toyota', 'Celica', 1975);
    */

    query += "INSERT INTO " + tableName;

    // Add the columns
    query += "(";

    for (int col = 0; col < columns.size(); col++)
    {
        query += std::get<1>(columns[col]);
        if (col != columns.size() - 1)
        {
            query += ",";
        }
    }

    query += ")\n";

    // Add the values
    query += "VALUES\n";

    for (int row = 0; row < data.size(); row++)
    {
        query += "(";
        for (int col = 0; col < data[row].size(); col++)
        {
            /*
            
    columns.push_back(std::tuple<std::string, std::string>("TIMESTAMP", "Timestamp"));
    columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Source"));
    columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Destination"));
    columns.push_back(std::tuple<std::string, std::string>("INT", "Length"));

            */
            if (std::get<0>(columns[col]) == "TIMESTAMP")
            {
                query += "to_timestamp(" + data[row][col] + "::double precision / 1000)";
            }
            else if (std::get<0>(columns[col]) == "VARCHAR(255)")
            {
                query += "'" + data[row][col] + "'";
            }
            else if (std::get<0>(columns[col]) == "INT")
            {
                query += data[row][col];
            }

            if (col != data[row].size() - 1)
            {
                query += ",";
            }
        }
        query += ")";
        if (row != data.size() - 1)
        {
            query += ",";
        }
        query += "\n";
    }
    query += ";";

    return query;
}