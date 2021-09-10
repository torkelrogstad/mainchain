// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/managenewsdialog.h>
#include <qt/forms/ui_managenewsdialog.h>

#include <txdb.h>
#include <validation.h>

ManageNewsDialog::ManageNewsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageNewsDialog)
{
    ui->setupUi(this);

    UpdateTypes();
}

ManageNewsDialog::~ManageNewsDialog()
{
    delete ui;
}

void ManageNewsDialog::on_pushButtonWrite_clicked()
{
    // Create header bytes
    std::vector<unsigned char> vBytes = ParseHex(ui->lineEditBytes->text().toStdString());

    // Copy header bytes into OP_RETURN script
    CScript script;
    script.resize(vBytes.size() + 1);
    script[0] = OP_RETURN;
    memcpy(&script[1], vBytes.data(), vBytes.size());

    // New custom news type object
    CustomNewsType custom;
    custom.title = ui->lineEditTitle->text().toStdString();
    custom.header = script;

    // Save new custom type
    popreturndb->WriteCustomType(custom);

    // Tell widgets we have updated custom types
    Q_EMIT(NewTypeCreated());
    UpdateTypes();
}

void ManageNewsDialog::UpdateTypes()
{
    ui->listWidget->clear();

    // Add the default types to the combo box
    ui->listWidget->addItem("All OP_RETURN data");
    ui->listWidget->addItem("Tokyo Daily News");
    ui->listWidget->addItem("US Daily News");

    // Add the custom types to the combo box

    std::vector<CustomNewsType> vCustom;
    popreturndb->GetCustomTypes(vCustom);

    for (const CustomNewsType c : vCustom) {
        std::string strHex = "(" + HexStr(c.header.begin(), c.header.end()) + ")";
        QString label = QString::fromStdString(c.title + " | " + strHex);
        ui->listWidget->addItem(label);
    }
}
