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

/// @brief Init
PSQLShell::PSQLShell(std::string dbName, std::string user, std::string passkey)
{
    if (INSTANCE != nullptr)
    {
        return;
    }

    INSTANCE = this;
    bool initialized = AttemptInitShell(dbName, user, passkey);

    if (initialized) {
        std::cout << "After Init Shell" << std::endl;

        std::string tableName = "TestTable1";
        std::vector<std::tuple<std::string, std::string>> columns;
        columns.push_back(std::tuple<std::string, std::string>("TIMESTAMP", "Timestamp"));
        columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Source"));
        columns.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Destination"));
        columns.push_back(std::tuple<std::string, std::string>("INT", "Length"));
        // columns.push_back(std::tuple<std::string, std::string>("INT", "TestColumn2_Int"));

        // std::cout << "[" << timestamp << "] "
        //           << "Src: " << src_addr << " -> "
        //           << "Dst: " << dest_addr << " | "
        //           << "Proto: " << protocol_name << " | "
        //           << "Len: " << packet_len << " bytes" << std::endl;

        std::cout << "After Column Init" << std::endl;

        DatabaseHelper = new PSQLDatabaseHelper(tableName, columns);

        std::cout << "DatabaseHelper created" << std::endl;

        std::string initTableQuery = DatabaseHelper->CreateInitializeTableQuery();

        Write(initTableQuery);

        std::cout << "-> Initialize Table Query:" << std::endl
                << initTableQuery << std::endl;

        // std::vector<std::vector<std::string>> data;
        // std::vector<std::string> row1;
        // row1.push_back("Test Name");
        // data.push_back(row1);

        // std::string addPacketQuery = DatabaseHelper->CreateAddPacketQuery(data);
        // Write(addPacketQuery);
        // // row1.push_back("5");
        // std::cout << "-> Test Add Packet Data:" << std::endl << addPacketQuery << std::endl;

        // Hook into the capture stream
        // capture_callback_add(CaptureCallback, (gpointer) this);

        // Register a tap to hook into the packet data stream
        std::cout << "Calling RegisterTap" << std::endl;
        RegisterTap();
    }
}

/// @brief Delister
PSQLShell::~PSQLShell()
{
    INSTANCE = nullptr;
    if (shellPipe)
    {
#if defined(_WIN32) || defined(_WIN64)
        _pclose(shellPipe);
#elif defined(__linux__)
        pclose(shellPipe);
#endif
    }
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

/// @brief Also a delister
void PSQLShell::HandleDisconnect()
{
    delete PSQLShell::INSTANCE;
}

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

int PSQLShell::dissect_psql(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
    std::cout << "Called dissect_psql" << std::endl;
    struct PSQLTap *psqlInfo = wmem_new0(pinfo->pool, struct PSQLTap);
    psqlInfo->packet_type = tvb_get_guint8(tvb, 0);
    psqlInfo->priority = tvb_get_ntohs(tvb, 8);

    std::cout << "> Packet Type: " << psqlInfo->packet_type << std::endl;
    std::cout << "> Priority: " << psqlInfo->priority << std::endl;

    tap_queue_packet(psql_tap, pinfo, psqlInfo);

    // Get time
    double timestamp = (double)pinfo->abs_ts.secs * 1000 + ((double)pinfo->abs_ts.nsecs / 1000000.0);

    // Get source, destination
    std::string src_addr = address_to_str(wmem_packet_scope(), &(pinfo->src));
    std::string dest_addr = address_to_str(wmem_packet_scope(), &(pinfo->dst));

    // Get protocol layer
    std::string protocol_name = pinfo->current_proto;

    // Get packet length
    guint32 packet_len = pinfo->fd->pkt_len;

    // Here's our data!
    std::cout << "[" << timestamp << "] "
              << "Src: " << src_addr << " -> "
              << "Dst: " << dest_addr << " | "
              << "Proto: " << protocol_name << " | "
              << "Len: " << packet_len << " bytes" << std::endl;

    std::cout << "Finished dissection for dissect_psql" << std::endl;

    std::vector<std::vector<std::string>> data2;
    std::vector<std::string> row1;
    row1.push_back(std::to_string(timestamp));
    row1.push_back(src_addr);
    row1.push_back(dest_addr);
    row1.push_back(std::to_string(packet_len));
    data2.push_back(row1);
    std::string addPacketQuery = DatabaseHelper->CreateAddPacketQuery(data2);
    INSTANCE->Write(addPacketQuery);

    return tvb_captured_length(tvb);
}