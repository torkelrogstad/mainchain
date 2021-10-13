// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NEWSQRDIALOG_H
#define NEWSQRDIALOG_H

#include <QDialog>

namespace Ui {
class NewsQRDialog;
}

class NewsQRDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewsQRDialog(QWidget *parent = nullptr);
    ~NewsQRDialog();

    void SetURL(const QString& url);

private:
    Ui::NewsQRDialog *ui;
};

#endif // NEWSQRDIALOG_H
