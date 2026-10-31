#include "psql_shell.h"

#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>

PSQLShell *PSQLShell::INSTANCE = nullptr;
PSQLDatabaseHelper *PSQLShell::PacketsTable = nullptr;
PSQLDatabaseHelper *PSQLShell::TCPTable = nullptr;
PSQLDatabaseHelper *PSQLShell::UDPTable = nullptr;
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
        INSTANCE = nullptr;
        return;
    }

    AttemptInitDatabase();
}

/// @brief Delister
PSQLShell::~PSQLShell()
{
    INSTANCE = nullptr;

    if (PacketsTable)
    {
        delete PacketsTable;
        PacketsTable = nullptr;
    }

    if (TCPTable)
    {
        delete TCPTable;
        TCPTable = nullptr;
    }

    if (UDPTable)
    {
        delete UDPTable;
        UDPTable = nullptr;
    }

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
    if (INSTANCE == nullptr)
    {
        return;
    }

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
    // Packets -----------------------------------------------------------------------------------
    std::vector<std::tuple<std::string, std::string>> columnsPackets;
    columnsPackets.push_back(std::tuple<std::string, std::string>("INT", "Number"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("TIMESTAMP", "Timestamp"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Source"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Destination"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("VARCHAR(255)", "Protocol"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("INT", "Length"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("INT", "SourcePort"));
    columnsPackets.push_back(std::tuple<std::string, std::string>("INT", "DestinationPort"));

    PacketsTable = new PSQLDatabaseHelper("Packets", columnsPackets);
    Write(PacketsTable->CreateInitializeTableQuery());

    // TCP -----------------------------------------------------------------------------------
    std::vector<std::tuple<std::string, std::string>> columnsTCP;
    columnsTCP.push_back(std::tuple<std::string, std::string>("INT", "Number"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("TIMESTAMP", "Timestamp"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("BIGINT", "SeqNumber"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("BIGINT", "AckNumber"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("BIGINT", "SeqNumberRaw"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("BIGINT", "AckNumberRaw"));
    columnsTCP.push_back(std::tuple<std::string, std::string>("BIGINT", "WindowNumber"));

    TCPTable = new PSQLDatabaseHelper("TCP", columnsTCP);
    Write(TCPTable->CreateInitializeTableQuery());

    // Register a tap to hook into the packet data stream
    std::cout << "Calling RegisterTap" << std::endl;
    RegisterTap();
}

/// @brief Write to PSQL shell, attempts to handle disconnects if no shell is active
bool PSQLShell::Write(const std::string &input)
{
    if (INSTANCE == nullptr || shellPipe == nullptr)
    {
        return false;
    }

    std::cout << "Received input:\n-------------------------------\n"
              << input << std::endl;

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

    const char *wantedFieldNames[] = {
        "tcp.seq",
        "tcp.ack",
        "tcp.seq_raw",
        "tcp.ack_raw",
        "tcp.window_size_value"};
    GArray *wantedFields = g_array_sized_new(FALSE, FALSE, sizeof(int), G_N_ELEMENTS(wantedFieldNames));
    for (guint i = 0; i < G_N_ELEMENTS(wantedFieldNames); i++)
    {
        header_field_info *field = proto_registrar_get_byname(wantedFieldNames[i]);
        if (field == nullptr)
        {
            g_warning("PatchSQL: TCP field %s is not registered", wantedFieldNames[i]);
            continue;
        }

        g_array_append_val(wantedFields, field->id);
    }
    set_postdissector_wanted_hfids(psql_handle, wantedFields);

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

    // Write To Packets Table
    std::string number = std::to_string(pinfo->num);
    std::string timestamp = std::to_string((double)pinfo->abs_ts.secs * 1000 + ((double)pinfo->abs_ts.nsecs / 1000000.0));
    std::string src_addr = address_to_str(wmem_packet_scope(), &(pinfo->src));
    std::string dest_addr = address_to_str(wmem_packet_scope(), &(pinfo->dst));
    std::string protocol_name = GetProtocolField(pinfo);
    std::string packet_len = std::to_string(pinfo->fd->pkt_len);
    std::string src_port = std::to_string(pinfo->srcport);
    std::string dest_port = std::to_string(pinfo->destport);

    std::vector<std::vector<std::string>> packet_table;
    std::vector<std::string> packet_tuple;

    packet_tuple.push_back(number);
    packet_tuple.push_back(timestamp);
    packet_tuple.push_back(src_addr);
    packet_tuple.push_back(dest_addr);
    packet_tuple.push_back(protocol_name);
    packet_tuple.push_back(packet_len);
    packet_tuple.push_back(src_port);
    packet_tuple.push_back(dest_port);

    packet_table.push_back(packet_tuple);
    INSTANCE->Write(PacketsTable->CreateAddPacketQuery(packet_table));

    // Write To TCP Table
    if (pinfo->ptype == PT_TCP)
    {
        std::string seq_num = "0";
        std::string ack_num = "0";
        std::string seq_num_raw = "0";
        std::string ack_num_raw = "0";
        std::string win_num_raw = "0";
        GetFieldValue(tree, "tcp.seq", seq_num);
        GetFieldValue(tree, "tcp.ack", ack_num);
        GetFieldValue(tree, "tcp.seq_raw", seq_num_raw);
        GetFieldValue(tree, "tcp.ack_raw", ack_num_raw);
        GetFieldValue(tree, "tcp.window_size_value", win_num_raw);
        std::vector<std::vector<std::string>> tcp_table;
        std::vector<std::string> tcp_tuple;

        tcp_tuple.push_back(number);
        tcp_tuple.push_back(timestamp);
        tcp_tuple.push_back(seq_num);
        tcp_tuple.push_back(ack_num);
        tcp_tuple.push_back(seq_num_raw);
        tcp_tuple.push_back(ack_num_raw);
        tcp_tuple.push_back(win_num_raw);

        tcp_table.push_back(tcp_tuple);
        INSTANCE->Write(TCPTable->CreateAddPacketQuery(tcp_table));
    }

    return tvb_captured_length(tvb);
}

/// @brief Because getting the correct protocol is stupidly complex
std::string PSQLShell::GetProtocolField(packet_info *pinfo)
{
    std::string r = "N/A";
    for (wmem_list_frame_t *layer = wmem_list_head(pinfo->layers); layer != nullptr; layer = wmem_list_frame_next(layer))
    {
        int proto_id = GPOINTER_TO_INT(wmem_list_frame_data(layer));
        if (proto_id == proto_psql)
        {
            continue;
        }

        protocol_t *protocol = find_protocol_by_id(proto_id);
        if (protocol == nullptr || proto_is_pino(protocol))
        {
            continue;
        }

        const char *short_name = proto_get_protocol_short_name(protocol);
        if (short_name == nullptr ||
            g_strcmp0(short_name, "Frame") == 0 ||
            g_strcmp0(short_name, "Data") == 0)
        {
            continue;
        }
        r = short_name;
    }
    return r;
}

/// @brief Grabs fields within tree
bool PSQLShell::GetFieldValue(proto_tree *tree, const char *fieldName, std::string &value)
{
    if (tree == nullptr || fieldName == nullptr)
    {
        return false;
    }

    header_field_info *field = proto_registrar_get_byname(fieldName);
    if (field == nullptr)
    {
        return false;
    }

    GPtrArray *fieldInfos = proto_get_finfo_ptr_array(tree, field->id);
    if (fieldInfos == nullptr || fieldInfos->len == 0)
    {
        return false;
    }

    field_info *fieldInfo = static_cast<field_info *>(g_ptr_array_index(fieldInfos, 0));
    value = std::to_string(fvalue_get_uinteger(&fieldInfo->value));
    return true;
}
