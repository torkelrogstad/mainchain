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
    CustomNewsType custom;
    custom.title = ui->lineEditTitle->text().toStdString();

    // Create header bytes
    std::vector<unsigned char> vBytes = ParseHex(ui->lineEditBytes->text().toStdString());
    CScript script;
    script.resize(vBytes.size() + 1);
    script[0] = OP_RETURN;
    memcpy(&script[1], &vBytes, vBytes.size());

    custom.header = script;

    popreturndb->WriteCustomType(custom);

    Q_EMIT(NewTypeCreated());
    UpdateTypes();
}

void ManageNewsDialog::UpdateTypes()
{
    ui->listWidget->clear();

    ui->listWidget->addItem("All OP_RETURN data");
    ui->listWidget->addItem("Tokyo Daily News");
    ui->listWidget->addItem("US Daily News");

    std::vector<CustomNewsType> vCustom;
    popreturndb->GetCustomTypes(vCustom);

    for (const CustomNewsType c : vCustom)
        ui->listWidget->addItem(QString::fromStdString(c.title));
}
