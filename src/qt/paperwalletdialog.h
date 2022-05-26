// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAPERWALLETDIALOG_H
#define PAPERWALLETDIALOG_H

#include <QDialog>

#include <uint256.h>

class PlatformStyle;

namespace Ui {
class PaperWalletDialog;
}

QT_BEGIN_NAMESPACE
class QString;
class QListWidgetItem;
QT_END_NAMESPACE

// Word list table columns
enum
{
    COLUMN_BIN = 0,
    COLUMN_INDEX,
    COLUMN_WORD,
};

// Word list table column width
enum
{
    COLUMN_BIN_WIDTH = 160,
    COLUMN_INDEX_WIDTH = 80,
    COLUMN_WORD_WIDTH = 150,
};

struct WordTableObject
{
    QString bin;
    QString index;
    QString word;
};

class PaperWalletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaperWalletDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~PaperWalletDialog();

private Q_SLOTS:
    void on_pushButtonPrint_clicked();
    void on_pushButtonHelp_clicked();
    void on_lineEditEntropy_textChanged(QString text);

private:
    Ui::PaperWalletDialog *ui;

    const PlatformStyle *platformStyle;
    std::vector<WordTableObject> vWords;

    std::vector<WordTableObject> EntropyToWordList(const std::vector<unsigned char>& vchEntropy, const std::vector<unsigned char>& vchEntropyHash);

    void UpdateWords();
};

bool EntropyToKeys(const std::vector<unsigned char>& vchEntropy, std::string& strXPub, std::string& strXPriv, std::vector<std::string>& vChildAddr);


#endif // PAPERWALLETDIALOG_H
