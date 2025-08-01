/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <QFileDialog>
#include <QMenu>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"
#include "DSi_NAND.h"

#include "TitleManagerDialog.h"
#include "ui_TitleManagerDialog.h"
#include "ui_TitleImportDialog.h"

using namespace melonDS;
using namespace melonDS::Platform;

std::unique_ptr<DSi_NAND::NANDImage> TitleManagerDialog::nand = nullptr;
TitleManagerDialog* TitleManagerDialog::currentDlg = nullptr;


TitleManagerDialog::TitleManagerDialog(QWidget* parent, DSi_NAND::NANDImage& image) : QDialog(parent), ui(new Ui::TitleManagerDialog), nandmount(image)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    ui->lstTitleList->setIconSize(QSize(32, 32));

    const u32 category = 0x00030004;
    std::vector<u32> titlelist;
    nandmount.ListTitles(category, titlelist);

    for (std::vector<u32>::iterator it = titlelist.begin(); it != titlelist.end(); it++)
    {
        u32 titleid = *it;
        createTitleItem(category, titleid);
    }

    ui->lstTitleList->sortItems();

    ui->btnImportTitleData->setEnabled(false);
    ui->btnExportTitleData->setEnabled(false);
    ui->btnDeleteTitle->setEnabled(false);

    {
        QMenu* menu = new QMenu(ui->btnImportTitleData);

        actImportTitleData[0] = menu->addAction("public.sav");
        actImportTitleData[0]->setData(QVariant(DSi_NAND::TitleData_PublicSav));
        connect(actImportTitleData[0], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        actImportTitleData[1] = menu->addAction("private.sav");
        actImportTitleData[1]->setData(QVariant(DSi_NAND::TitleData_PrivateSav));
        connect(actImportTitleData[1], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        actImportTitleData[2] = menu->addAction("banner.sav");
        actImportTitleData[2]->setData(QVariant(DSi_NAND::TitleData_BannerSav));
        connect(actImportTitleData[2], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        ui->btnImportTitleData->setMenu(menu);
    }

    {
        QMenu* menu = new QMenu(ui->btnExportTitleData);

        actExportTitleData[0] = menu->addAction("public.sav");
        actExportTitleData[0]->setData(QVariant(DSi_NAND::TitleData_PublicSav));
        connect(actExportTitleData[0], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        actExportTitleData[1] = menu->addAction("private.sav");
        actExportTitleData[1]->setData(QVariant(DSi_NAND::TitleData_PrivateSav));
        connect(actExportTitleData[1], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        actExportTitleData[2] = menu->addAction("banner.sav");
        actExportTitleData[2]->setData(QVariant(DSi_NAND::TitleData_BannerSav));
        connect(actExportTitleData[2], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        ui->btnExportTitleData->setMenu(menu);
    }
}

TitleManagerDialog::~TitleManagerDialog()
{
    delete ui;
}

void TitleManagerDialog::createTitleItem(u32 category, u32 titleid)
{
    u32 version;
    NDSHeader header;
    NDSBanner banner;

    nandmount.GetTitleInfo(category, titleid, version, &header, &banner);

    u32 icondata[32*32];
    emuInstance->romIcon(banner.Icon, banner.Palette, icondata);
    QImage iconimg((const uchar*)icondata, 32, 32, QImage::Format_RGBA8888);
    QIcon icon(QPixmap::fromImage(iconimg.copy()));

    // TODO: make it possible to select other languages?
    QString title = QString::fromUtf16(banner.EnglishTitle, 128);
    title = title.left(title.indexOf('\0'));
    title.replace("\n", " · ");

    char gamecode[5];
    *(u32*)&gamecode[0] = *(u32*)&header.GameCode[0];
    gamecode[4] = '\0';
    char extra[128];
    snprintf(extra, sizeof(extra), "\n(应用 ID: %s · %08x/%08x · 版本 %08x)", gamecode, category, titleid, version);

    QListWidgetItem* item = new QListWidgetItem(title + QString(extra));
    item->setIcon(icon);
    item->setData(Qt::UserRole, QVariant((qulonglong)(((u64)category<<32) | (u64)titleid)));
    item->setData(Qt::UserRole+1, QVariant(header.DSiPublicSavSize)); // public.sav size
    item->setData(Qt::UserRole+2, QVariant(header.DSiPrivateSavSize)); // private.sav size
    item->setData(Qt::UserRole+3, QVariant((u32)((header.AppFlags & 0x04) ? 0x4000 : 0))); // banner.sav size
    ui->lstTitleList->addItem(item);
}

bool TitleManagerDialog::openNAND()
{
    nand = nullptr;

    Config::Table cfg = Config::GetGlobalTable();

    FileHandle* bios7i = Platform::OpenLocalFile(cfg.GetString("DSi.BIOS7Path"), FileMode::Read);
    if (!bios7i)
        return false;

    u8 es_keyY[16];
    FileSeek(bios7i, 0x8308, FileSeekOrigin::Start);
    FileRead(es_keyY, 16, 1, bios7i);
    CloseFile(bios7i);

    FileHandle* nandfile = Platform::OpenLocalFile(cfg.GetString("DSi.NANDPath"), FileMode::ReadWriteExisting);
    if (!nandfile)
        return false;

    nand = std::make_unique<DSi_NAND::NANDImage>(nandfile, es_keyY);
    if (!*nand)
    { // If loading and mounting the NAND image failed...
        nand = nullptr;
        return false;
        // NOTE: The NANDImage takes ownership of the FileHandle,
        // so it will be closed even if the NANDImage constructor fails.
    }

    return true;
}

void TitleManagerDialog::closeNAND()
{
    nand = nullptr;
}

void TitleManagerDialog::done(int r)
{
    QDialog::done(r);

    closeDlg();
}

void TitleManagerDialog::on_btnImportTitle_clicked()
{
    TitleImportDialog* importdlg = new TitleImportDialog(this, importAppPath, &importTmdData, importReadOnly, nandmount);
    importdlg->open();
    connect(importdlg, &TitleImportDialog::finished, this, &TitleManagerDialog::onImportTitleFinished);

    importdlg->show();
}

void TitleManagerDialog::onImportTitleFinished(int res)
{
    if (res != QDialog::Accepted) return;

    u32 titleid[2];
    titleid[0] = importTmdData.GetCategory();
    titleid[1] = importTmdData.GetID();

    assert(nand != nullptr);
    assert(*nand);
    // remove anything that might hinder the install
    nandmount.DeleteTitle(titleid[0], titleid[1]);

    bool importres = nandmount.ImportTitle(importAppPath.toStdString().c_str(), importTmdData, importReadOnly);
    if (!importres)
    {
        // remove a potential half-completed install
        nandmount.DeleteTitle(titleid[0], titleid[1]);

        QMessageBox::critical(this,
                              "导入应用 - melonDS",
                              "将应用安装到NAND时出错。\n检查您的NAND转储是否有效。");
    }
    else
    {
        // it worked, wee!
        createTitleItem(titleid[0], titleid[1]);
        ui->lstTitleList->sortItems();
    }
}

void TitleManagerDialog::on_btnDeleteTitle_clicked()
{
    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur) return;

    if (QMessageBox::question(this,
                              "删除应用 - melonDS",
                              "应用及其相关数据将被永久删除。 你确定吗？",
                              QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No),
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    nandmount.DeleteTitle((u32)(titleid >> 32), (u32)titleid);

    delete cur;
}

void TitleManagerDialog::on_lstTitleList_currentItemChanged(QListWidgetItem* cur, QListWidgetItem* prev)
{
    if (!cur)
    {
        ui->btnImportTitleData->setEnabled(false);
        ui->btnExportTitleData->setEnabled(false);
        ui->btnDeleteTitle->setEnabled(false);
    }
    else
    {
        ui->btnImportTitleData->setEnabled(true);
        ui->btnExportTitleData->setEnabled(true);
        ui->btnDeleteTitle->setEnabled(true);

        u32 val;
        val = cur->data(Qt::UserRole+1).toUInt();
        actImportTitleData[0]->setEnabled(val != 0);
        actExportTitleData[0]->setEnabled(val != 0);
        val = cur->data(Qt::UserRole+2).toUInt();
        actImportTitleData[1]->setEnabled(val != 0);
        actExportTitleData[1]->setEnabled(val != 0);
        val = cur->data(Qt::UserRole+3).toUInt();
        actImportTitleData[2]->setEnabled(val != 0);
        actExportTitleData[2]->setEnabled(val != 0);
    }
}

void TitleManagerDialog::onImportTitleData()
{
    int type = ((QAction*)sender())->data().toInt();

    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur)
    {
        Log(LogLevel::Error, "什么??\n");
        return;
    }

    QString extensions = "*.sav";
    u32 wantedsize;
    switch (type)
    {
    case DSi_NAND::TitleData_PublicSav:
        extensions += " *.pub";
        wantedsize = cur->data(Qt::UserRole+1).toUInt();
        break;
    case DSi_NAND::TitleData_PrivateSav:
        extensions += " *.prv";
        wantedsize = cur->data(Qt::UserRole+2).toUInt();
        break;
    case DSi_NAND::TitleData_BannerSav:
        extensions += " *.bnr";
        wantedsize = cur->data(Qt::UserRole+3).toUInt();
        break;
    default:
        Log(LogLevel::Warn, "what??\n");
        return;
    }

    QString file = QFileDialog::getOpenFileName(this,
                                                "选择导入文件...",
                                                emuDirectory,
                                                "应用数据文件 (" + extensions + ");;任何文件 (*.*)");

    if (file.isEmpty()) return;

    Platform::FileHandle* f = Platform::OpenFile(file.toStdString(), Platform::Read);
    if (!f)
    {
        QMessageBox::critical(this,
                              "导入应用数据 - melonDS",
                              "无法打开数据文件。\n检查文件是否可访问。");
        return;
    }

    u64 len = Platform::FileLength(f);
    Platform::CloseFile(f);

    if (len != wantedsize)
    {
        QMessageBox::critical(this,
                              "导入应用数据 - melonDS",
                              QString("无法导入此数据文件：大小不正确 (预期： %1 bytes).").arg(wantedsize));
        return;
    }

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    bool res = nandmount.ImportTitleData((u32)(titleid >> 32), (u32)titleid, type, file.toStdString().c_str());
    if (!res)
    {
        QMessageBox::critical(this,
                              "导入应用数据 - melonDS",
                              "导入数据文件失败。 检查您的NAND是否可访问且有效。");
    }
}

void TitleManagerDialog::onExportTitleData()
{
    int type = ((QAction*)sender())->data().toInt();

    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur)
    {
        Log(LogLevel::Error, "什么??\n");
        return;
    }

    QString exportname;
    QString extensions = "*.sav";
    u32 wantedsize;
    switch (type)
    {
    case DSi_NAND::TitleData_PublicSav:
        exportname = "/public.sav";
        extensions += " *.pub";
        wantedsize = cur->data(Qt::UserRole+1).toUInt();
        break;
    case DSi_NAND::TitleData_PrivateSav:
        exportname = "/private.sav";
        extensions += " *.prv";
        wantedsize = cur->data(Qt::UserRole+2).toUInt();
        break;
    case DSi_NAND::TitleData_BannerSav:
        exportname = "/banner.sav";
        extensions += " *.bnr";
        wantedsize = cur->data(Qt::UserRole+3).toUInt();
        break;
    default:
        Log(LogLevel::Warn, "what??\n");
        return;
    }

    QString file = QFileDialog::getSaveFileName(this,
                                                "选择导出路径...",
                                                emuDirectory + exportname,
                                                "应用数据文件 (" + extensions + ");;任何文件 (*.*)");

    if (file.isEmpty()) return;

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    bool res = nandmount.ExportTitleData((u32)(titleid >> 32), (u32)titleid, type, file.toStdString().c_str());
    if (!res)
    {
        QMessageBox::critical(this,
                              "导出应用数据 - melonDS",
                              "无法导出数据文件。 检查目标目录是否可写。");
    }
}


TitleImportDialog::TitleImportDialog(QWidget* parent, QString& apppath, const DSi_TMD::TitleMetadata* tmd, bool& readonly, DSi_NAND::NANDMount& nandmount)
: QDialog(parent), ui(new Ui::TitleImportDialog), appPath(apppath), tmdData(tmd), readOnly(readonly), nandmount(nandmount)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    grpTmdSource = new QButtonGroup(this);
    grpTmdSource->addButton(ui->rbTmdFromFile, 0);
    grpTmdSource->addButton(ui->rbTmdFromNUS, 1);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(grpTmdSource, SIGNAL(buttonClicked(int)), this, SLOT(onChangeTmdSource(int)));
#else
    connect(grpTmdSource, SIGNAL(idClicked(int)), this, SLOT(onChangeTmdSource(int)));
#endif
    grpTmdSource->button(0)->setChecked(true);
}

TitleImportDialog::~TitleImportDialog()
{
    delete ui;
}

void TitleImportDialog::accept()
{
    QString path;
    FILE* f;

    bool tmdfromfile = (grpTmdSource->checkedId() == 0);

    path = ui->txtAppFile->text();
    f = fopen(path.toStdString().c_str(), "rb");
    if (!f)
    {
        QMessageBox::critical(this,
                              "导入应用 - melonDS",
                              "无法打开可执行文件。\n请检查路径是否正确以及文件是否可访问。");
        return;
    }

    fseek(f, 0x230, SEEK_SET);
    fread(titleid, 8, 1, f);
    fclose(f);

    if (titleid[1] != 0x00030004)
    {
        QMessageBox::critical(this,
                              "导入应用 - melonDS",
                              "可执行文件不是DSiWare应用。");
        return;
    }

    if (tmdfromfile)
    {
        path = ui->txtTmdFile->text();
        f = fopen(path.toStdString().c_str(), "rb");
        if (!f)
        {
            QMessageBox::critical(this,
                                  "导入应用 - melonDS",
                                  "无法打开元数据文件。\n请检查路径是否正确以及文件是否可访问。");
            return;
        }

        fread((void *) tmdData, sizeof(DSi_TMD::TitleMetadata), 1, f);
        fclose(f);

        u32 tmdtitleid[2];
        tmdtitleid[0] = tmdData->GetCategory();
        tmdtitleid[1] = tmdData->GetID();

        if (tmdtitleid[1] != titleid[0] || tmdtitleid[0] != titleid[1])
        {
            QMessageBox::critical(this,
                                  "导入应用 - melonDS",
                                  "元数据文件中的应用ID与可执行文件不匹配。");
            return;
        }
    }

    if (nandmount.TitleExists(titleid[1], titleid[0]))
    {
        if (QMessageBox::question(this,
                                  "导入应用 - melonDS",
                                  "所选应用已安装。 覆盖它？",
                                  QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No),
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    if (!tmdfromfile)
    {
        network = new QNetworkAccessManager(this);

        char url[256];
        snprintf(url, sizeof(url), "http://nus.cdn.t.shop.nintendowifi.net/ccs/download/%08x%08x/tmd", titleid[1], titleid[0]);

        QNetworkRequest req;
        req.setUrl(QUrl(url));

        netreply = network->get(req);
        connect(netreply, &QNetworkReply::finished, this, &TitleImportDialog::tmdDownloaded);

        setEnabled(false);
    }
    else
    {
        appPath = ui->txtAppFile->text();
        readOnly = ui->cbReadOnly->isChecked();
        QDialog::accept();
    }
}

void TitleImportDialog::tmdDownloaded()
{
    bool good = false;

    if (netreply->error() != QNetworkReply::NoError)
    {
        QMessageBox::critical(this,
                              "导入应用 - melonDS",
                              QString("尝试下载元数据文件时出错：\n\n") + netreply->errorString());
    }
    else if (netreply->bytesAvailable() < 2312)
    {
        QMessageBox::critical(this,
                              "导入应用 - melonDS",
                              "NUS 返回了格式错误的元数据文件。");
    }
    else
    {
        netreply->read((char*)tmdData, sizeof(*tmdData));

        u32 tmdtitleid[2];
        tmdtitleid[0] = tmdData->GetCategory();
        tmdtitleid[1] = tmdData->GetID();

        if (tmdtitleid[1] != titleid[0] || tmdtitleid[0] != titleid[1])
        {
            QMessageBox::critical(this,
                                  "导入应用 - melonDS",
                                  "NUS 返回了格式错误的元数据文件。");
        }
        else
            good = true;
    }

    netreply->deleteLater();
    setEnabled(true);

    if (good)
    {
        appPath = ui->txtAppFile->text();
        readOnly = ui->cbReadOnly->isChecked();
        QDialog::accept();
    }
}

void TitleImportDialog::on_btnAppBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "选择可执行应用...",
                                                emuDirectory,
                                                "DSiWare可执行程序 (*.app *.nds *.dsi *.srl);;任何文件 (*.*)");

    if (file.isEmpty()) return;

    ui->txtAppFile->setText(file);
}

void TitleImportDialog::on_btnTmdBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "选择应用元数据...",
                                                emuDirectory,
                                                "DSiWare元数据 (*.tmd);;任何文件 (*.*)");

    if (file.isEmpty()) return;

    ui->txtTmdFile->setText(file);
}

void TitleImportDialog::onChangeTmdSource(int id)
{
    bool pathenable = (id==0);

    ui->txtTmdFile->setEnabled(pathenable);
    ui->btnTmdBrowse->setEnabled(pathenable);
}
