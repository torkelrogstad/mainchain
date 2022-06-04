// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <qt/paperwalletdialog.h>
#include <qt/forms/ui_paperwalletdialog.h>

#include <qt/guiutil.h>
#include <qt/hashcalcdialog.h> // For HexToBinStr TODO remove
#include <qt/platformstyle.h>

#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
//#include <QPdfDocument> // TODO update Qt version?
#include <QPrinter>
#include <QPrintDialog>
#include <QScrollBar>
#include <QString>
#include <QTextStream>

#include <base58.h>
#include <bip39words.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <key.h>
#include <utilstrencodings.h>
#include <wallet/wallet.h>

#include <bitset>

PaperWalletDialog::PaperWalletDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PaperWalletDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    // Setup word list table
    ui->tableWidgetWords->setColumnCount(3);
    ui->tableWidgetWords->setHorizontalHeaderLabels(
                QStringList() << "Bitstream" << "Index" << "Word");
    ui->tableWidgetWords->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    ui->tableWidgetWords->setColumnWidth(COLUMN_BIN, COLUMN_BIN_WIDTH);
    ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_WIDTH);
    ui->tableWidgetWords->setColumnWidth(COLUMN_WORD, COLUMN_WORD_WIDTH);

    ui->tableWidgetWords->horizontalHeader()->setStretchLastSection(true);
}

PaperWalletDialog::~PaperWalletDialog()
{
    delete ui;
}

void PaperWalletDialog::on_pushButtonCopyWords_clicked()
{
    QString words = "";
    for (const WordTableObject& o : vWords)
        words += o.word + " ";

    GUIUtil::setClipboard(words);
}

void PaperWalletDialog::on_pushButtonPrint_clicked()
{
    // TODO

    QPrinter printer;
    printer.setOutputFormat(QPrinter::NativeFormat);

    // Let user select printer or file destination
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QFile file(":/doc/paperwallet");

//    QPdfDocument pdf;

//    printer.setOutputFormat(QPrinter::PdfFormat);

    // Setting this will make it print to a file instead of going to spools
//    printer.setOutputFileName("file.pdf");

//    QPdfWriter writer(&printer);

//    QPainter painter;
//    if (!painter.begin(&printer))
//        return;

//    painter.drawText(10, 10, "Hello printer!");

//    if (!printer.newPage())
//        return;

//    painter.drawText(10, 10, "Hello second page!");
//    painter.end();
}

void PaperWalletDialog::on_pushButtonHelp_clicked()
{
    QMessageBox messageBox;
    messageBox.setWindowTitle("Paper Wallet Help");

    QString error = "Currently you may use this page to generate BIP 39 mnemonic wordlists.\n";
    error += "12, 15, 18, 21, 24 word lists may be generated.\n\n";
    error += "Enter any hexidecimal value of the correct length depending on how many words you want to generate.\n\n";
    error += "Entropy input requirements:\n";
    error += "12 words: 128 bits\n";
    error += "15 words: 160 bits\n";
    error += "18 words: 192 bits\n";
    error += "21 words: 224 bits\n";
    error += "24 words: 256 bits\n\n";
    error += "More features will be enabled later!\n";

    messageBox.setText(error);
    messageBox.exec();
}

void PaperWalletDialog::on_lineEditEntropy_textChanged(QString text)
{
    // Clear old info
    ui->tableWidgetWords->setUpdatesEnabled(false);
    ui->tableWidgetWords->setRowCount(0);
    ui->tableWidgetWords->setUpdatesEnabled(true);
    ui->textBrowserAddress->clear();
    ui->labelXPub->setText("");
    ui->labelXPriv->setText("");

    std::string str = text.toStdString();

    // TODO warn
    if (!IsHex(str))
        return;

    std::vector<unsigned char> vBytes = ParseHex(str);

    // TODO warn
    if (!vBytes.size() || (vBytes.size() * 8) % 32)
        return;

    // Get sha256 hash of entropy
    std::vector<unsigned char> vHash;
    vHash.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vHash[0]);

    // Generate xpub, xpriv, child addresses
    std::string strXPub = "";
    std::string strXPriv = "";
    std::vector<std::string> vChildAddr;
    if (EntropyToKeys(vBytes, strXPub, strXPriv, vChildAddr)) {
        // Display child addresses
        for (const std::string& str : vChildAddr)
            ui->textBrowserAddress->append(QString::fromStdString(str));

        // Scroll back to first address
        ui->textBrowserAddress->verticalScrollBar()->setValue(0);

        ui->labelXPub->setText(QString::fromStdString(strXPub.substr(0, 36)) + "...");
        ui->labelXPriv->setText(QString::fromStdString(strXPriv.substr(0, 36)) + "...");
    } else {
        ui->textBrowserAddress->clear();
        ui->labelXPub->setText("");
        ui->labelXPriv->setText("");
    }

    // Generate new mnemonic word list
    vWords = EntropyToWordList(vBytes, vHash);

    UpdateWords();
}

void PaperWalletDialog::UpdateWords()
{
    ui->tableWidgetWords->setUpdatesEnabled(false);
    ui->tableWidgetWords->setRowCount(0);

    int nRow = 0;
    for (const WordTableObject& o : vWords) {
        ui->tableWidgetWords->insertRow(nRow);

        // Bin
        QTableWidgetItem *itemBin = new QTableWidgetItem();
        itemBin->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // Split binary into groups
        QString binary = o.bin;
        if (binary.size() == 11) {
            binary.insert(3, " ");
            binary.insert(8, " ");
        }

        itemBin->setText(binary);
        itemBin->setFlags(itemBin->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(nRow, COLUMN_BIN, itemBin);

        // Index
        QTableWidgetItem *itemIndex = new QTableWidgetItem();
        itemIndex->setTextAlignment(Qt::AlignRight| Qt::AlignVCenter);
        itemIndex->setText(o.index + " ");
        itemIndex->setFlags(itemIndex->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(nRow, COLUMN_INDEX, itemIndex);

        // Word
        QString word = "";

        if (nRow < 9)
            word += " ";

        word += QString::number(nRow + 1) + ". ";
        word += o.word;

        QTableWidgetItem *itemWord = new QTableWidgetItem();
        itemWord->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        itemWord->setText(word);
        itemWord->setFlags(itemWord->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(nRow, COLUMN_WORD, itemWord);

        nRow++;
    }

    ui->tableWidgetWords->setUpdatesEnabled(true);
}

std::vector<WordTableObject> PaperWalletDialog::EntropyToWordList(const std::vector<unsigned char>& vchEntropy, const std::vector<unsigned char>& vchEntropyHash)
{
    // Number of bits must be a multiple of 32
    if (!vchEntropy.size() || (vchEntropy.size() * 8) % 32)
        return std::vector<WordTableObject>();

    // SHA256 hash of entropy must be 32 bytes
    if (vchEntropyHash.size() != 32)
        return std::vector<WordTableObject>();

    std::string strBits = HexToBinStr(HexStr(vchEntropy.begin(), vchEntropy.end()));
    std::string strHashBits = HexToBinStr(HexStr(vchEntropyHash.begin(), vchEntropyHash.end()));

    // The number of check bits is the number of bits / 32
    size_t nCheckBits = ((vchEntropy.size() * 8) / 32);

    // Copy nCheckBits from the hash of the entropy to the end of the entropy
    strBits += strHashBits.substr(0, nCheckBits);

    std::vector<WordTableObject> vWord;
    for (size_t i = 0; i < strBits.size(); i += 11) {
        // Convert 11 bits into an integer
        std::bitset<11> indexBits(strBits, i, 11);
        ulong num = indexBits.to_ulong();

        // Create WordTableObject
        QString bin = QString::fromStdString(indexBits.to_string());
        QString index = QString::number(num);
        QString word = QString::fromStdString(vBip39Word[num]);

        WordTableObject object;
        object.bin = bin;
        object.index = index;
        object.word = word;

        vWord.push_back(object);
    }

    return vWord;
}

bool EntropyToKeys(const std::vector<unsigned char>& vchEntropy, std::string& strXPub, std::string& strXPriv, std::vector<std::string>& vChildAddr) {
    // TODO

    // Make HD master key

    if (vchEntropy.size() != 32)
        return false;

    CKey key; // 256 bit Master key seed
    key.Set(vchEntropy.begin(), vchEntropy.end(), false);
    if (!key.IsValid())
        return false;

    // HD master key
    CExtKey masterKey;
    masterKey.SetMaster(key.begin(), key.size());

    // calculate the pubkey
    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));

    // Make HD child keys

    CExtKey accountKey;            // key at m/0'
    CExtKey chainChildKey;         // key at m/0'/0' (external) or m/0'/1' (internal)
    CExtKey childKey;              // key at m/0'/0'/<n>'

    // Derive account key m/0'
    // use hardened derivation (child keys >= 0x80000000 are hardened after bip32)
    masterKey.Derive(accountKey, 0x80000000);

    // Derive child chain key at m/0'/0' (external chain)
    accountKey.Derive(chainChildKey, 0x80000000);

    // TODO Fix: child keys & CBitcoinExtPubKey are generated incorrectly,
    // results do not match test vectors...

    for (size_t i = 0; i < 100; i++) {
        // Derive child key at m/0'/0'/<n>'
        chainChildKey.Derive(childKey, i | 0x80000000);

        CTxDestination dest = GetDestinationForKey(childKey.key.GetPubKey(), OUTPUT_TYPE_LEGACY);
        std::string strDest = EncodeDestination(dest);

        vChildAddr.push_back(strDest);
    }

    CBitcoinExtKey ext;
    ext.SetKey(masterKey);

    CBitcoinExtPubKey extpub;
    extpub.SetKey(masterKey.Neuter());

    strXPub = extpub.ToString();
    strXPriv = ext.ToString();

    return true;
}
