#ifndef PSQL_SHELL_H
#define PSQL_SHELL_H

#include <glib.h>
#include "ui/capture.h"
// #include "ui/qt/capture_event.h"
// #include "ui/qt/capture_file.h"

#include <string>

class PSQLShell
{
public:
    static PSQLShell *INSTANCE;

    PSQLShell(std::string dbName, std::string user, std::string passkey);
    ~PSQLShell();
    bool Write(const std::string& input);
\
private:
    FILE * shellPipe;

    bool AttemptInitShell(std::string dbName, std::string user, std::string passkey);
    void HandleDisconnect();

    static void CaptureCallback(gint event, capture_session *cap_session, gpointer user_data);
};

#endif // PSQLSHELL_H