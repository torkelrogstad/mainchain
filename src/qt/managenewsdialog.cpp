// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/managenewsdialog.h>
#include <qt/forms/ui_managenewsdialog.h>

#include <QMenu>
#include <QPoint>

#include <qt/guiutil.h>

#include <txdb.h>
#include <validation.h>

static const int NUM_DEFAULT = 3;

ManageNewsDialog::ManageNewsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageNewsDialog)
{
    ui->setupUi(this);

    ui->listWidgetTypes->setContextMenuPolicy(Qt::CustomContextMenu);

    // Context menu
    QAction *shareAction = new QAction(tr("Copy sharing URL"), this);
    contextMenu = new QMenu(this);
    contextMenu->setObjectName("contextMenuManageNews");
    contextMenu->addAction(shareAction);

    connect(ui->listWidgetTypes, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(shareAction, SIGNAL(triggered()), this, SLOT(copyShareURL()));

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
    custom.nDays = ui->spinBoxDays->value();

    // Save new custom type
    popreturndb->WriteCustomType(custom);

    // Tell widgets we have updated custom types
    Q_EMIT(NewTypeCreated());
    UpdateTypes();
}

void ManageNewsDialog::on_pushButtonPaste_clicked()
{
    ui->lineEditURL->setText(GUIUtil::getClipboard());
}

void ManageNewsDialog::on_pushButtonAdd_clicked()
{
    QString url = ui->lineEditURL->text();
    CustomNewsType custom;
    custom.SetURL(url.toStdString());

    // Save shared custom type
    popreturndb->WriteCustomType(custom);

    // Tell widgets we have updated custom types
    Q_EMIT(NewTypeCreated());
    UpdateTypes();
}

void ManageNewsDialog::UpdateTypes()
{
    vCustomCache.clear();
    ui->listWidgetTypes->clear();

    // Add the default types to the combo box
    ui->listWidgetTypes->addItem("All OP_RETURN data");
    ui->listWidgetTypes->addItem("Tokyo Daily News");
    ui->listWidgetTypes->addItem("US Daily News");

    // Add the custom types to the combo box
    popreturndb->GetCustomTypes(vCustomCache);

    for (const CustomNewsType c : vCustomCache) {
        std::string strHex = " {" + HexStr(c.header.begin(), c.header.end()) + "} ";
        QString label = QString::fromStdString(c.title);
        label += QString::fromStdString(strHex);
        label += QString::number(c.nDays) + " day(s)";
        ui->listWidgetTypes->addItem(label);
    }
}

void ManageNewsDialog::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->listWidgetTypes->indexAt(point);
    if (index.isValid() && (index.row() >= NUM_DEFAULT))
        contextMenu->popup(ui->listWidgetTypes->viewport()->mapToGlobal(point));
}

void ManageNewsDialog::copyShareURL()
{
    if (!ui->listWidgetTypes->selectionModel())
        return;

    QModelIndexList selection = ui->listWidgetTypes->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return;

    QModelIndex index = selection.front();
    if (!index.isValid())
        return;

    int nRow = index.row();
    nRow -= NUM_DEFAULT;

    if (nRow >= 0 && nRow <= (int)vCustomCache.size()) {
        QString url = QString::fromStdString(vCustomCache[nRow].GetShareURL());
        GUIUtil::setClipboard(url);
    }
}
