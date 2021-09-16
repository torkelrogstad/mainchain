// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MANAGENEWSDIALOG_H
#define MANAGENEWSDIALOG_H

#include <QDialog>

namespace Ui {
class ManageNewsDialog;
}

QT_BEGIN_NAMESPACE
class QMenu;
class QPoint;
QT_END_NAMESPACE

class CustomNewsType;

class ManageNewsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageNewsDialog(QWidget *parent = nullptr);
    ~ManageNewsDialog();

private Q_SLOTS:
    void on_pushButtonWrite_clicked();
    void on_pushButtonAdd_clicked();
    void on_pushButtonPaste_clicked();
    void contextualMenu(const QPoint &);
    void copyShareURL();

Q_SIGNALS:
    void NewTypeCreated();

private:
    Ui::ManageNewsDialog *ui;
    void UpdateTypes();

    QMenu *contextMenu;

    std::vector<CustomNewsType> vCustomCache;
};

#endif // MANAGENEWSDIALOG_H
