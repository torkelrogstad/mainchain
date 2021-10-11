// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/managenewsdialog.h>
#include <qt/forms/ui_managenewsdialog.h>

#include <QMenu>
#include <QMessageBox>
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
    ui->tableViewTypes->resizeColumnsToContents();

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

    std::string strTitle = ui->lineEditTitle->text().toStdString();
    if (strTitle.empty()) {
        QMessageBox::critical(this, tr("Failed to create news type"),
            tr("News type must have a title!\n"),
            QMessageBox::Ok);
        return;
    }

    std::string strBytes = ui->lineEditBytes->text().toStdString();
    if (!IsHexNumber(strBytes)) {
        QMessageBox::critical(this, tr("Failed to create news type"),
            tr("Invalid header bytes!\n\n"
               "Header bytes must be four valid hexidecimal characters with no prefix."),
            QMessageBox::Ok);
        return;
    }

    // Create header bytes
    std::vector<unsigned char> vBytes = ParseHex(strBytes);

    // Copy header bytes into OP_RETURN script
    CScript script(vBytes.begin(), vBytes.end());

    if (script.size() != 4) {
        QMessageBox::critical(this, tr("Failed to create news type"),
            tr("Invalid hex bytes length!\n\n"
               "Header bytes must be four valid hexidecimal characters with no prefix."),
            QMessageBox::Ok);
        return;
    }

    // New news type object
    NewsType type;
    type.title = strTitle;
    type.header = script;
    type.nDays = ui->spinBoxDays->value();

    // Save new type
    popreturndb->WriteNewsType(type);

    // Tell widgets we have updated custom types
    newsTypesModel->updateModel();
    Q_EMIT(NewTypeCreated());

    QMessageBox::information(this, tr("News type created"),
        tr("News type created!"),
        QMessageBox::Ok);
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
    if (!type.SetURL(url.toStdString())) {
        QMessageBox::critical(this, tr("Failed to add news type"),
            tr("Invalid news type URL!\n"),
            QMessageBox::Ok);
        return;
    }

    // Save shared custom type
    popreturndb->WriteNewsType(type);

    // Tell widgets we have updated custom types
    newsTypesModel->updateModel();
    Q_EMIT(NewTypeCreated());

    QMessageBox::information(this, tr("News type added"),
        tr("News type added!"),
        QMessageBox::Ok);
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
    ui->tableViewTypes->resizeColumnsToContents();
}
