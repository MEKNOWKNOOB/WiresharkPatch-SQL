#include "psql_shell.h"

#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>

PSQLShell *PSQLShell::INSTANCE = nullptr;

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