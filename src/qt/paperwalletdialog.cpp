// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <qt/paperwalletdialog.h>
#include <qt/forms/ui_paperwalletdialog.h>

#include <qt/guiutil.h>
#include <qt/hashcalcdialog.h> // For HexToBinStr & BinToHexStr
#include <qt/platformstyle.h>

#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
//#include <QPdfDocument> // TODO update Qt version?
//#include <QPrinter>
//#include <QPrintDialog>
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

    ui->tableWidgetWords->setUpdatesEnabled(false);
    ui->tableWidgetWords->setRowCount(0);
    for (size_t i = 0; i < 12; i++) {
        ui->tableWidgetWords->insertRow(i);

        // Bin
        QTableWidgetItem *itemBin = new QTableWidgetItem();
        itemBin->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        itemBin->setText("");
        itemBin->setFlags(itemBin->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(i, COLUMN_BIN, itemBin);

        // Index
        QTableWidgetItem *itemIndex = new QTableWidgetItem();
        itemIndex->setTextAlignment(Qt::AlignRight| Qt::AlignVCenter);
        itemIndex->setText("");
        itemIndex->setFlags(itemIndex->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(i, COLUMN_INDEX, itemIndex);

        // Word
        QTableWidgetItem *itemWord = new QTableWidgetItem();
        itemWord->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        itemWord->setText("");
        itemBin->setFlags(itemWord->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetWords->setItem(i, COLUMN_WORD, itemWord);
    }
    ui->tableWidgetWords->setUpdatesEnabled(true);

    ui->radioButtonCreate->setChecked(true);
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

    // QPrinter printer;
    // printer.setOutputFormat(QPrinter::NativeFormat);

    // Let user select printer or file destination
    // QPrintDialog dialog(&printer, this);
    // if (dialog.exec() != QDialog::Accepted)
    //    return;

    // QFile file(":/doc/paperwallet");

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

    messageBox.setText(error);
    messageBox.exec();
}

void PaperWalletDialog::Clear()
{
    // Clear old info

    ui->textBrowserAddress->clear();
    ui->textBrowserSeed->setPlainText("");
    ui->labelXPub->setText("");
    ui->labelXPriv->setText("");
    ui->labelv3->setText("");

    ui->tableWidgetWords->setUpdatesEnabled(false);
    for (size_t i = 0; i < 12; i++) {
        ui->tableWidgetWords->item(i, COLUMN_BIN)->setText("");
        ui->tableWidgetWords->item(i, COLUMN_INDEX)->setText("");
        ui->tableWidgetWords->item(i, COLUMN_WORD)->setText("");
    }
    ui->tableWidgetWords->setUpdatesEnabled(true);
}

void PaperWalletDialog::on_lineEditEntropy_textChanged(QString text)
{   
    Clear();

    std::string str = text.toStdString();
    if (str.empty())
        return;

    // Generate sha256 hash of plain text to use as entropy
    std::vector<unsigned char> vEntropy;
    vEntropy.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&str[0], str.size()).Finalize(&vEntropy[0]);

    // Cut bytes down to the BIP39 entropy requirement for 12 words
    vEntropy.resize(16);

    std::string strEntropy = HexStr(vEntropy);

    // Show entropy on GUI
    ui->textBrowserSeed->insertPlainText("  bip39 hex: " + QString::fromStdString(strEntropy) + "\n\n");

    // Show entropy in decimal
    std::vector<int> vDec;
    for (const unsigned char& c : std::string(strEntropy)) {
        int digit = QString(c).toInt(nullptr, 16);

        for (size_t i = 0; i < vDec.size(); i++) {
            int val = vDec[i] * 16 + digit;
            vDec[i] = val % 10;
            digit = val / 10;
        }

        while (digit) {
            vDec.push_back(digit % 10);
            digit /= 10;
        }
    }
    QByteArray array;
    for (size_t i = 0; i < vDec.size(); i++) {
        array.prepend((char)('0' + vDec[i]));
    }
    ui->textBrowserSeed->insertPlainText("  bip39 dec: " + QString(array) + "\n");

    // Generate sha256 hash of entropy bytes to create BIP39 mnemonic check bits
    std::vector<unsigned char> vCheck;
    vCheck.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&vEntropy[0], vEntropy.size()).Finalize(&vCheck[0]);

    const size_t nCheckBits = 4;
    const std::string strBits = HexToBinStr(HexStr(vEntropy.begin(), vEntropy.end()));
    const std::string strCheck = HexStr(vCheck.begin(), vCheck.end());
    const std::string strCheckBits = HexToBinStr(strCheck).substr(0, nCheckBits);

    // Split binary into groups of 4
    std::string strBinSpaced = "";
    for (size_t i = 0; i < strBits.size(); i += 4) {
        strBinSpaced += strBits.substr(i, 4) + " ";
    }

    ui->textBrowserSeed->append("&nbsp;&nbsp;bip39 bin: " + QString::fromStdString(strBinSpaced) +
                                "<font color=\"hotpink\">" + QString::fromStdString(strCheckBits) + "</font><br>");

    // Show checksum bits and the partial hex character the bits represent
    ui->textBrowserSeed->append("&nbsp;bip39 csum: \'" + QString::fromStdString(strCheck.substr(0, 1)) + "\' <font color=\"hotpink\">"
                                + QString::fromStdString(strCheckBits) + "</font><br><br>");

    // Show HD wallet input (sha256 hash of entropy)
    ui->textBrowserSeed->insertPlainText("HD key data: " + QString::fromStdString(HexStr(vCheck.begin(), vCheck.end())) + "\n");

    // Generate xpub, xpriv, child addresses
    std::string strXPub = "";
    std::string strXPriv = "";
    std::string strV3 = "";
    std::vector<std::string> vChildAddr;
    if (EntropyToKeys(vCheck, strXPub, strXPriv, strV3, vChildAddr)) {
        // Display child addresses
        for (const std::string& str : vChildAddr)
            ui->textBrowserAddress->append(QString::fromStdString(str));

        // Scroll back to first address
        ui->textBrowserAddress->verticalScrollBar()->setValue(0);

        ui->labelXPub->setText(QString::fromStdString(strXPub));
        ui->labelXPriv->setText(QString::fromStdString(strXPriv.substr(0, 36)) + "...");
        ui->labelv3->setText(QString::fromStdString(strV3));
    } else {
        ui->textBrowserAddress->clear();
        ui->labelXPub->setText("");
        ui->labelXPriv->setText("");
        ui->labelv3->setText("");
    }

    // Scroll back to top of seed / entropy output
    ui->textBrowserSeed->verticalScrollBar()->setValue(0);

    // Generate new mnemonic word list
    vWords = EntropyToWordList(vEntropy, vCheck);

    ui->tableWidgetWords->setUpdatesEnabled(false);
    for (size_t i = 0; i < vWords.size(); i++) {
        // Bin
        // Split binary into groups
        QString binary = vWords[i].bin;
        if (binary.size() == 11) {
            binary.insert(3, " ");
            binary.insert(8, " ");
        }
        ui->tableWidgetWords->item(i, COLUMN_BIN)->setText(binary);

        ui->tableWidgetWords->item(i, COLUMN_INDEX)->setText(vWords[i].index + " ");

        QString word = "";
        if (i < 9)
            word += " ";
        word += QString::number(i + 1) + ". ";
        word += vWords[i].word;
        ui->tableWidgetWords->item(i, COLUMN_WORD)->setText(word);
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

void PaperWalletDialog::on_radioButtonCreate_toggled(bool fChecked)
{
    if (!fChecked)
        return;

    Clear();

    ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_WIDTH);

    ui->lineEditEntropy->setEnabled(true);
    ui->lineEditEntropy->setPlaceholderText("Enter plain text to generate 256 bit entropy hash");
    ui->pushButtonCopyWords->setEnabled(true);

    ui->tableWidgetWords->setUpdatesEnabled(false);
    for (size_t i = 0; i < 12; i++)  {
        QTableWidgetItem* item = ui->tableWidgetWords->item(i, COLUMN_WORD);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
    ui->tableWidgetWords->setUpdatesEnabled(true);
}

void PaperWalletDialog::on_radioButtonRestore_toggled(bool fChecked)
{
    if (!fChecked)
        return;

    ui->lineEditEntropy->clear();
    ui->lineEditEntropy->setEnabled(false);
    ui->lineEditEntropy->setPlaceholderText("");
    ui->pushButtonCopyWords->setEnabled(false);

    Clear();

    ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_RESTORE_WIDTH);

    ui->tableWidgetWords->setUpdatesEnabled(false);
    for (size_t i = 0; i < 12; i++) {
        ui->tableWidgetWords->item(i, COLUMN_INDEX)->setText("Enter word " + QString::number(i + 1) + ":");

        QTableWidgetItem* itemWord = ui->tableWidgetWords->item(i, COLUMN_WORD);
        itemWord->setFlags(itemWord->flags() | Qt::ItemIsEditable);
    }
    ui->tableWidgetWords->setUpdatesEnabled(true);

    ui->textBrowserSeed->insertPlainText("Please enter 12 word seed on table below.");
}

void PaperWalletDialog::on_tableWidgetWords_itemChanged(QTableWidgetItem* item)
{
    if (!ui->radioButtonRestore->isChecked())
        return;

    if (item->column() != COLUMN_WORD)
        return;

    // Check if word is in word list and insert data if it is

    int nRow = item->row();
    QString text = item->text();

    if (text.isEmpty()) {
        ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_RESTORE_WIDTH);
        ui->tableWidgetWords->item(nRow, COLUMN_BIN)->setText("");
        ui->tableWidgetWords->item(nRow, COLUMN_INDEX)->setText("Enter word " + QString::number(nRow + 1) + ":");
        return;
    }

    // Check if text is bip39 word
    auto it = mapBip39Index.find(text.toStdString());
    if (it == mapBip39Index.end()) {
        // Stop if item text is a complete word prepended by word #
        int nLastSpace = text.lastIndexOf(" ");
        if (nLastSpace && (nLastSpace + 1) < text.size()) {
            QString sub = text.mid(nLastSpace + 1, text.size());
            if (mapBip39Index.count(sub.toStdString()))
                return;
        }

        ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_RESTORE_WIDTH);
        ui->tableWidgetWords->item(nRow, COLUMN_BIN)->setText("");
        ui->tableWidgetWords->item(nRow, COLUMN_INDEX)->setText("Enter word " + QString::number(nRow + 1) + ":");
        return;
    }

    const unsigned int index = it->second;
    std::bitset<11> indexBits(index);

    // Split binary into groups
    QString binary = QString::fromStdString(indexBits.to_string());
    if (binary.size() == 11) {
        binary.insert(3, " ");
        binary.insert(8, " ");
    }

    // Word
    QString word = "";
    if (nRow < 9)
        word.prepend(" ");

    word += QString::number(nRow + 1) + ". ";
    word += text;

    ui->tableWidgetWords->item(nRow, COLUMN_BIN)->setText(binary);
    ui->tableWidgetWords->item(nRow, COLUMN_INDEX)->setText(QString::number(index) + " ");
    ui->tableWidgetWords->item(nRow, COLUMN_WORD)->setText(word);

    // Check if all words are entered and update output if they are

    bool fComplete = true;
    for (size_t i = 0; i < 12; i++) {
        QTableWidgetItem* item = ui->tableWidgetWords->item(i, COLUMN_WORD);
        QString text = item->text();

        if (text.isEmpty()) {
            fComplete = false;
            break;
        }

        int nLastSpace = text.lastIndexOf(" ");
        if (nLastSpace && (nLastSpace + 1) < text.size()) {
            QString sub = text.mid(nLastSpace + 1, text.size());
            if (!mapBip39Index.count(sub.toStdString())) {
                fComplete = false;
                break;
            }
        }
    }

    if (!fComplete)
        return;

    ui->tableWidgetWords->setColumnWidth(COLUMN_INDEX, COLUMN_INDEX_WIDTH);

    ui->textBrowserSeed->clear();

    QString bin = "";
    for (size_t i = 0; i < 12; i++) {
        QTableWidgetItem* item = ui->tableWidgetWords->item(i, COLUMN_BIN);
        bin += item->text();
    }
    // Remove incorrect spaces
    bin.replace(" ", "");

    // Binary -> Hex
    std::string strBin = bin.toStdString();
    std::string strHex = BinToHexStr(strBin);
    std::vector<unsigned char> vEntropy = ParseHex(strHex);

    // Format binary for GUI with correct spacing
    QString binSpaced = "";
    for (int i = 0; i < bin.size(); i += 4)
        binSpaced += bin.mid(i, 4) + " ";

    // Show entropy on GUI
    ui->textBrowserSeed->insertPlainText("  bip39 hex: " + QString::fromStdString(strHex) + "\n\n");

    // Show entropy in decimal
    std::vector<int> vDec;
    for (const unsigned char& c : std::string(strHex)) {
        int digit = QString(c).toInt(nullptr, 16);

        for (size_t i = 0; i < vDec.size(); i++) {
            int val = vDec[i] * 16 + digit;
            vDec[i] = val % 10;
            digit = val / 10;
        }

        while (digit) {
            vDec.push_back(digit % 10);
            digit /= 10;
        }
    }
    QByteArray array;
    for (size_t i = 0; i < vDec.size(); i++) {
        array.prepend((char)('0' + vDec[i]));
    }
    ui->textBrowserSeed->insertPlainText("  bip39 dec: " + QString(array) + "\n");

    // Generate sha256 hash of entropy bytes to create BIP39 mnemonic check bits
    std::vector<unsigned char> vCheck;
    vCheck.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&vEntropy[0], vEntropy.size()).Finalize(&vCheck[0]);

    const size_t nCheckBits = 4;
    const std::string strBits = HexToBinStr(HexStr(vEntropy.begin(), vEntropy.end()));
    const std::string strCheck = HexStr(vCheck.begin(), vCheck.end());
    const std::string strCheckBits = HexToBinStr(strCheck).substr(0, nCheckBits);

    ui->textBrowserSeed->append("&nbsp;&nbsp;bip39 bin: " + binSpaced.mid(0, binSpaced.size() - 5) +
                                "<font color=\"hotpink\">" + binSpaced.mid(binSpaced.size() - 5, binSpaced.size()) + "</font><br>");

    // Show checksum bits and the partial hex character the bits represent
    ui->textBrowserSeed->append("&nbsp;bip39 csum: \'" + QString::fromStdString(strCheck.substr(0, 1)) + "\' <font color=\"hotpink\">"
                                + QString::fromStdString(strCheckBits) + "</font><br><br>");

    // Show HD wallet input (sha256 hash of entropy)
    ui->textBrowserSeed->insertPlainText("HD key data: " + QString::fromStdString(HexStr(vCheck.begin(), vCheck.end())) + "\n");

    // Generate xpub, xpriv, child addresses
    std::string strXPub = "";
    std::string strXPriv = "";
    std::string strV3 = "";
    std::vector<std::string> vChildAddr;
    if (EntropyToKeys(vCheck, strXPub, strXPriv, strV3, vChildAddr)) {
        // Display child addresses
        for (const std::string& str : vChildAddr)
            ui->textBrowserAddress->append(QString::fromStdString(str));

        // Scroll back to first address
        ui->textBrowserAddress->verticalScrollBar()->setValue(0);

        ui->labelXPub->setText(QString::fromStdString(strXPub));
        ui->labelXPriv->setText(QString::fromStdString(strXPriv.substr(0, 36)) + "...");
        ui->labelv3->setText(QString::fromStdString(strV3));
    } else {
        ui->textBrowserAddress->clear();
        ui->labelXPub->setText("");
        ui->labelXPriv->setText("");
        ui->labelv3->setText("");
    }
}

bool EntropyToKeys(const std::vector<unsigned char>& vchEntropy, std::string& strXPub, std::string& strXPriv, std::string& strV3, std::vector<std::string>& vChildAddr) {
    // TODO

    // Make HD master key

    if (vchEntropy.size() != 32)
        return false;

    CKey key; // 256 bit Master key seed
    key.Set(vchEntropy.begin(), vchEntropy.end(), true);
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

    for (size_t i = 0; i < 100; i++) {
        // Derive child key at m/0'/0'/<n>'
        chainChildKey.Derive(childKey, i | 0x80000000);

        CTxDestination dest = GetDestinationForKey(childKey.key.GetPubKey(), OUTPUT_TYPE_LEGACY);
        std::string strDest = EncodeDestination(dest);

        std::string strPriv = CBitcoinSecret(childKey.key).ToString();


        std::string strPad = "";
        if (i < 10)
            strPad = "  ";
        else
        if (i < 100)
            strPad = " ";
//        else
//        if (i < 1000)
//            strPad = "   ";

        vChildAddr.push_back("m/0\'/0\'/" + std::to_string(i) + "'" + strPad + strPriv.substr(0, 10) + "... " + strDest);
    }

    CBitcoinExtKey ext;
    ext.SetKey(masterKey);

    CBitcoinExtPubKey extpub;
    extpub.SetKey(masterKey.Neuter());

    strXPub = extpub.ToString();
    strXPriv = ext.ToString();

    // Make payment code v3
    std::vector<unsigned char> vchV3;
    vchV3.resize(35);
    vchV3[0] = 0x22;
    vchV3[1] = 0x03;
    memcpy(&vchV3[2], pubkey.begin(), 33);

    strV3 = EncodeBase58Check(vchV3);

    return true;
}
