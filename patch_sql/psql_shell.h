#ifndef PSQL_SHELL_H
#define PSQL_SHELL_H

#include <glib.h>
#include "ui/capture.h"
#include <epan/packet.h>
#include <epan/tap.h>
#include <epan/address.h>
#include <epan/to_str.h>
#include "psql_database_helper.h"

#include <string>

struct PSQLTap
{
    int packet_type;
    int priority;
};

class PSQLShell
{
public:
    static PSQLShell *INSTANCE;
    static PSQLDatabaseHelper *DatabaseHelper;
    static int proto_psql;
    static dissector_handle_t psql_handle;
    static int psql_tap;

    PSQLShell(std::string dbName, std::string user, std::string passkey);
    ~PSQLShell();
    bool Write(const std::string& input);

    static int dissect_psql(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_);

private:
    FILE * shellPipe = nullptr;

    bool AttemptInitShell(std::string dbName, std::string user, std::string passkey);
    void AttemptInitDatabase();
    void HandleDisconnect();
    void RegisterTap();
};

#endif // PSQLSHELL_H