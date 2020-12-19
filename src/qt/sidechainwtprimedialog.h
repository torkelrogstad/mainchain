// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINWTPRIMEDIALOG_H
#define SIDECHAINWTPRIMEDIALOG_H

#include <QDialog>

class PlatformStyle;
class WTPrimeVoteTableModel;

namespace Ui {
class SidechainWTPrimeDialog;
}

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SidechainWTPrimeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainWTPrimeDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~SidechainWTPrimeDialog();

public Q_SLOTS:
    void on_pushButtonUpvote_clicked();

    void on_pushButtonDownvote_clicked();

    void on_pushButtonAbstain_clicked();

    void on_pushButtonHelp_clicked();

    void on_comboBoxDefaultVote_currentIndexChanged(const int i);

    void Update();

private:
    Ui::SidechainWTPrimeDialog *ui;

    WTPrimeVoteTableModel *wtPrimeVoteModel = nullptr;

    const PlatformStyle *platformStyle;

    QTimer *pollTimer;

};

#endif // SIDECHAINWTPRIMEDIALOG_H
