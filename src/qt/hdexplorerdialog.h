// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HDEXPLORERDIALOG_H
#define HDEXPLORERDIALOG_H

#include <QDialog>

class PlatformStyle;

namespace Ui {
class HDExplorerDialog;
}

class HDExplorerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HDExplorerDialog(const PlatformStyle *_platformStyle, QWidget *parent = nullptr);
    ~HDExplorerDialog();

private:
    Ui::HDExplorerDialog *ui;

    const PlatformStyle *platformStyle = nullptr;

private Q_SLOTS:
    void on_lineEditWords_textChanged(QString text);
};

#endif // HDEXPLORERDIALOG_H
