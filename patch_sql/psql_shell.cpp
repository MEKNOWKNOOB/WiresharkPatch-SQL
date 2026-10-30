#include "psql_shell.h"

#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>

PSQLShell *PSQLShell::INSTANCE = nullptr;
PSQLDatabaseHelper *PSQLShell::DatabaseHelper = nullptr;
int PSQLShell::proto_psql = -1;
dissector_handle_t PSQLShell::psql_handle = nullptr;

int PSQLShell::psql_tap = -1;

/// @brief Init
PSQLShell::PSQLShell(std::string dbName, std::string user, std::string passkey)
{
    if (INSTANCE != nullptr)
    {
        return;
    }

    INSTANCE = this;

    if (!AttemptInitShell(dbName, user, passkey))
    {
        return;
    }

    AttemptInitDatabase();
}

/// @brief Delister
PSQLShell::~PSQLShell()
{
    INSTANCE = nullptr;

    delete DatabaseHelper;
    DatabaseHelper = nullptr;

    if (shellPipe)
    {
#if defined(_WIN32) || defined(_WIN64)
        _pclose(shellPipe);
#elif defined(__linux__)
        pclose(shellPipe);
#endif
        shellPipe = nullptr;
    }
}

/// @brief Also a delister
void PSQLShell::HandleDisconnect()
{
    delete PSQLShell::INSTANCE;
}

/// @brief Attempts to connect to PSQL
bool PSQLShell::AttemptInitShell(std::string dbName, std::string user, std::string passkey)
{
#if defined(_WIN32) || defined(_WIN64)

    _putenv_s("PGPASSWORD", passkey.c_str());

    // init test to see if info is correct
    std::string testCmd = "psql -d " + dbName + " -U " + user + " -c \"SELECT 1;\" >nul 2>&1";
    if (std::system(testCmd.c_str()) != 0)
    {
        std::cerr << "Failed to establish connection" << std::endl;
        INSTANCE = nullptr;
        return false;
    }

    // actuall establish connection
    std::string cmd = "psql -d " + dbName + " -U " + user;
    shellPipe = _popen(cmd.c_str(), "w");
    if (!shellPipe)
    {
        std::cerr << "Failed to establish connection" << std::endl;
        return false;
    }

#elif defined(__linux__)

    setenv("PGPASSWORD", passkey.c_str(), 1);

    std::string testCmd = "psql -h localhost -d " + dbName + " -U " + user + " -c \"SELECT 1;\" >/dev/null 2>&1";
    if (std::system(testCmd.c_str()) != 0)
    {
        std::cerr << "Failed to establish connection" << std::endl;
        INSTANCE = nullptr;
        return false;
    }

    std::string cmd = "psql -h localhost -d " + dbName + " -U " + user;
    shellPipe = popen(cmd.c_str(), "w");
    if (!shellPipe)
    {
        std::cerr << "Failed to establish connection" << std::endl;
        return false;
    }

#else

    std::cerr << "Unsupported OS" << std::endl;
    return false;

#endif

    return true;
}

/// @brief Create PSQLTable and register Tap
void PSQLShell::AttemptInitDatabase()
{
    std::string tableName = "Packets";
    std::vector<std::tuple<std::string, std::string>> columns;
    columns.push_back(std::tuple<std::string, std::string>("INT", "Number"));
    columns.push_back(std::tuple<std::string, std::string>("TIMESTAMP", "Timestamp"));
    columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Source"));
    columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Destination"));
    columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Protocol"));
    columns.push_back(std::tuple<std::string, std::string>("INT", "Length"));

    std::cout << "Vector Table Packets Created" << std::endl;

    DatabaseHelper = new PSQLDatabaseHelper(tableName, columns);
    std::string initTableQuery = DatabaseHelper->CreateInitializeTableQuery();
    if (Write(initTableQuery))
    {
        std::cout << "-> DataTable Created" << std::endl
                  << initTableQuery << std::endl;
    }

    // Register a tap to hook into the packet data stream
    std::cout << "Calling RegisterTap" << std::endl;
    RegisterTap();
}

/// @brief Write to PSQL shell, attempts to handle disconnects if no shell is active
bool PSQLShell::Write(const std::string &input)
{
    if (!shellPipe)
    {
        return false;
    }

    std::cout << ">>> Received input: " << input << std::endl;

    if (fputs((input + "\n").c_str(), shellPipe) == EOF)
    {
        HandleDisconnect();
        return false;
    }
    if (fflush(shellPipe) != 0 || ferror(shellPipe))
    {
        HandleDisconnect();
        return false;
    }

    return true;
}

/// @brief Register Tap, the system that reads the packets
void PSQLShell::RegisterTap()
{
    if (proto_psql != -1)
    {
        // If we've already registered the tap, then don't do anything
        return;
    }

    std::cout << "Registering tap" << std::endl;
    proto_psql = proto_register_protocol("PatchSQL Protocol", "PSQL", "psql");
    psql_tap = register_tap("psql");
    psql_handle = register_dissector("psql", dissect_psql, proto_psql);

    register_postdissector(psql_handle);
    std::cout << "Tap registered" << std::endl;
}

/// @brief Confusing static function that register_dissector needs
int PSQLShell::dissect_psql(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
    if (INSTANCE == nullptr)
    {
        std::cout << "No PSQL shell active" << std::endl;
        return -1;
    }

    std::cout << "Called dissect_psql" << std::endl;

    struct PSQLTap *psqlInfo = wmem_new0(pinfo->pool, struct PSQLTap);
    psqlInfo->packet_type = tvb_get_guint8(tvb, 0);
    psqlInfo->priority = tvb_get_ntohs(tvb, 8);

    tap_queue_packet(psql_tap, pinfo, psqlInfo);

    // Get Data
    std::string number = std::to_string(pinfo->num);
    std::string timestamp = std::to_string((double)pinfo->abs_ts.secs * 1000 + ((double)pinfo->abs_ts.nsecs / 1000000.0));
    std::string src_addr = address_to_str(wmem_packet_scope(), &(pinfo->src));
    std::string dest_addr = address_to_str(wmem_packet_scope(), &(pinfo->dst));
    const gchar *raw_protocol_name = col_get_text(pinfo->cinfo, COL_PROTOCOL);
    std::string protocol_name = (raw_protocol_name) ? std::string(raw_protocol_name) : "N/A";
    std::string packet_len = std::to_string(pinfo->fd->pkt_len);

    // Here's our data!
    std::cout << number << " | "
              << "[" << timestamp << "] "
              << "Src: " << src_addr << " -> "
              << "Dst: " << dest_addr << " | "
              << "Proto: " << protocol_name << " | "
              << "Len: " << packet_len << " bytes" << std::endl;

    std::vector<std::vector<std::string>> table;
    std::vector<std::string> tuple;

    tuple.push_back(number);
    tuple.push_back(timestamp);
    tuple.push_back(src_addr);
    tuple.push_back(dest_addr);
    tuple.push_back(protocol_name);
    tuple.push_back(packet_len);

    table.push_back(tuple);

    std::string addPacketQuery = DatabaseHelper->CreateAddPacketQuery(table);
    INSTANCE->Write(addPacketQuery);

    return tvb_captured_length(tvb);
}
