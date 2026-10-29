#include "psql_shell.h"

#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>

PSQLShell *PSQLShell::INSTANCE = nullptr;
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

    if (fputs((input + "\n").c_str(), shellPipe))
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
    AttemptInitShell(dbName, user, passkey);

    // Hook into the capture stream
    // capture_callback_add(CaptureCallback, (gpointer) this);

    // Register a tap to hook into the packet data stream
    std::cout << "Calling RegisterTap" << std::endl;
    RegisterTap();
}

/// @brief Delister
PSQLShell::~PSQLShell()
{
    INSTANCE = nullptr;
    if (shellPipe)
    {
        _pclose(shellPipe);
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

    // do this later

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
    double timestamp = (double)pinfo->abs_ts.secs + ((double)pinfo->abs_ts.nsecs / 1000000000.0);

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

    return tvb_captured_length(tvb);
}