#ifndef PSQL_SHELL_H
#define PSQL_SHELL_H

#include <string>

class PSQLShell
{
public:
    static PSQLShell *INSTANCE;

    PSQLShell(std::string dbName, std::string user, std::string passkey);
    ~PSQLShell();
    bool Write(const std::string& input);

private:
    FILE * shellPipe;

    bool AttemptInitShell(std::string dbName, std::string user, std::string passkey);
    void HandleDisconnect();
};

#endif // PSQLSHELL_H