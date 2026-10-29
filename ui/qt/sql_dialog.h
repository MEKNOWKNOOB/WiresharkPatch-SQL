#ifndef SQL_DIALOG_H
#define SQL_DIALOG_H

#include <QDialog>
#include <QString>

namespace Ui
{
    class SqlDialog;
}

class SqlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SqlDialog(QWidget *parent = nullptr);
    ~SqlDialog();

    QString getDatabaseName() const;
    QString getUsername() const;
    QString getPasskey() const;

private slots:
    void on_pushButtonConnect_clicked();
    void on_pushButtonDisconnect_clicked();

private:
    Ui::SqlDialog *ui;
};

#endif // SQL_DIALOG_H