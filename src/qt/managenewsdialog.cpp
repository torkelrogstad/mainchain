// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/managenewsdialog.h>
#include <qt/forms/ui_managenewsdialog.h>

#include <QMenu>
#include <QPoint>

#include <qt/newstypestablemodel.h>
#include <qt/guiutil.h>

#include <txdb.h>
#include <validation.h>

ManageNewsDialog::ManageNewsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageNewsDialog)
{
    ui->setupUi(this);

    ui->tableViewTypes->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTypes->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->tableViewTypes->setContextMenuPolicy(Qt::CustomContextMenu);

    // Context menu
    QAction *shareAction = new QAction(tr("Copy sharing URL"), this);
    contextMenu = new QMenu(this);
    contextMenu->setObjectName("contextMenuManageNews");
    contextMenu->addAction(shareAction);

    connect(ui->tableViewTypes, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(shareAction, SIGNAL(triggered()), this, SLOT(copyShareURL()));
}

ManageNewsDialog::~ManageNewsDialog()
{
    delete ui;
}

void ManageNewsDialog::on_pushButtonWrite_clicked()
{
    if (!newsTypesModel)
        return;

    // Create header bytes
    std::vector<unsigned char> vBytes = ParseHex(ui->lineEditBytes->text().toStdString());

    // Copy header bytes into OP_RETURN script
    CScript script(vBytes.begin(), vBytes.end());

    // New news type object
    NewsType type;
    type.title = ui->lineEditTitle->text().toStdString();
    type.header = script;
    type.nDays = ui->spinBoxDays->value();

    // Save new type
    popreturndb->WriteNewsType(type);

    // Tell widgets we have updated custom types
    newsTypesModel->updateModel();
    Q_EMIT(NewTypeCreated());
}

void ManageNewsDialog::on_pushButtonPaste_clicked()
{
    ui->lineEditURL->setText(GUIUtil::getClipboard());
}

void ManageNewsDialog::on_pushButtonAdd_clicked()
{
    if (!newsTypesModel)
        return;

    QString url = ui->lineEditURL->text();
    NewsType type;
    type.SetURL(url.toStdString());

    // Save shared custom type
    popreturndb->WriteNewsType(type);

    // Tell widgets we have updated custom types
    newsTypesModel->updateModel();
    Q_EMIT(NewTypeCreated());
}

void ManageNewsDialog::contextualMenu(const QPoint& point)
{
    QModelIndex index = ui->tableViewTypes->indexAt(point);
    if (index.isValid())
        contextMenu->popup(ui->tableViewTypes->viewport()->mapToGlobal(point));
}

void ManageNewsDialog::copyShareURL()
{
    if (!newsTypesModel)
        return;

    if (!ui->tableViewTypes->selectionModel())
        return;

    QModelIndexList selection = ui->tableViewTypes->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return;

    QModelIndex index = selection.front();
    if (!index.isValid())
        return;

    int nRow = index.row();

    QString url = "";
    if (newsTypesModel->GetURLAtRow(nRow, url))
        GUIUtil::setClipboard(url);
}

void ManageNewsDialog::setNewsTypesModel(NewsTypesTableModel* model)
{
    newsTypesModel = model;
    ui->tableViewTypes->setModel(newsTypesModel);
}
