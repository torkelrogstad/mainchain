// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MANAGENEWSDIALOG_H
#define MANAGENEWSDIALOG_H

#include <QDialog>

namespace Ui {
class ManageNewsDialog;
}

class ManageNewsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageNewsDialog(QWidget *parent = nullptr);
    ~ManageNewsDialog();

private Q_SLOTS:
    void on_pushButtonWrite_clicked();

Q_SIGNALS:
    void NewTypeCreated();

private:
    Ui::ManageNewsDialog *ui;
    void UpdateTypes();
};

#endif // MANAGENEWSDIALOG_H
