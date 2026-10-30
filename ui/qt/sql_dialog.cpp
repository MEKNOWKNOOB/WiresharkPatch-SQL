#include "sql_dialog.h"

#include <ui_sql_dialog.h>
#include "patch_sql/psql_shell.h"
#include <QMessageBox>

SqlDialog::SqlDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SqlDialog)
{
    ui->setupUi(this);
}

SqlDialog::~SqlDialog()
{
    delete ui;
}

QString SqlDialog::getDatabaseName() const
{
    return ui->lineEditDbName->text();
}

QString SqlDialog::getUsername() const
{
    return ui->lineEditUsername->text();
}

QString SqlDialog::getPasskey() const
{
    return ui->lineEditPasskey->text();
}

/// @brief Attempts to create Instance PSQLShell 
void SqlDialog::on_pushButtonConnect_clicked()
{
    if (PSQLShell::INSTANCE != nullptr)
    {
        QMessageBox::information(this, "Already Connected", "PostgreSQL Session Already Active");
        return;
    }

    std::string dbName = getDatabaseName().toUtf8().constData();
    std::string user = getUsername().toUtf8().constData();
    std::string pass = getPasskey().toUtf8().constData();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    PSQLShell *shell = new PSQLShell(dbName, user, pass);
    QApplication::restoreOverrideCursor();

    if (PSQLShell::INSTANCE == nullptr)
    {
        std::cerr << "instance is null" << std::endl;
        QMessageBox::information(this, "Failed", "Failed To Connect to PostgreSQL Database");
        delete shell;
        std::cerr << "deleted shell" << std::endl;
        return;
    }

    QMessageBox::information(this, "Succeeded", "Connected to PostgreSQL Database");
}

/// @brief Attempts to delete Instance PSQLShell
void SqlDialog::on_pushButtonDisconnect_clicked()
{
    if (PSQLShell::INSTANCE == nullptr)
    {
        QMessageBox::information(this, "Failed", "No PostgreSQL Session");
        return;
    }

    delete PSQLShell::INSTANCE;
    QMessageBox::information(this, "Succeeded", "Closed PostgreSQL Session");
}