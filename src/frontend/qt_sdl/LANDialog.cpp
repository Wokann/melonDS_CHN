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
#include <string.h>
#include <queue>
#include <vector>

#include <QStandardItemModel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

#include "LANDialog.h"
#include "Config.h"
#include "main.h"
#include "LAN.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"

using namespace melonDS;


LANStartClientDialog* lanClientDlg = nullptr;
LANDialog* lanDlg = nullptr;

#define lan() ((LAN&)MPInterface::Get())


LANStartHostDialog::LANStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_LAN);

    auto cfg = Config::GetGlobalTable();
    ui->txtPlayerName->setText(cfg.GetQString("LAN.PlayerName"));

    ui->sbNumPlayers->setRange(2, 16);
    ui->sbNumPlayers->setValue(cfg.GetInt("LAN.HostNumPlayers"));
}

LANStartHostDialog::~LANStartHostDialog()
{
    delete ui;
}

void LANStartHostDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    if (r == QDialog::Accepted)
    {
        if (ui->txtPlayerName->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "melonDS", "请输入玩家名称。");
            return;
        }

        std::string player = ui->txtPlayerName->text().toStdString();
        int numplayers = ui->sbNumPlayers->value();

        if (!lan().StartHost(player.c_str(), numplayers))
        {
            QMessageBox::warning(this, "melonDS", "发起局域网(LAN)游戏失败。");
            return;
        }

        lanDlg = LANDialog::openDlg(parentWidget());

        auto cfg = Config::GetGlobalTable();
        cfg.SetString("LAN.PlayerName", player);
        cfg.SetInt("LAN.HostNumPlayers", numplayers);
        Config::Save();
    }
    else
    {
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}


LANStartClientDialog::LANStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_LAN);

    auto cfg = Config::GetGlobalTable();
    ui->txtPlayerName->setText(cfg.GetQString("LAN.PlayerName"));

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvAvailableGames->setModel(model);
    const QStringList listheader = {"名称", "玩家", "状态", "主机端 IP"};
    model->setHorizontalHeaderLabels(listheader);

    connect(ui->tvAvailableGames->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(onGameSelectionChanged(const QItemSelection&, const QItemSelection&)));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("连接");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    QPushButton* btn = ui->buttonBox->addButton("直接连接...", QDialogButtonBox::ActionRole);
    connect(btn, SIGNAL(clicked()), this, SLOT(onDirectConnect()));

    lanClientDlg = this;
    lan().StartDiscovery();

    timerID = startTimer(1000);
}

LANStartClientDialog::~LANStartClientDialog()
{
    killTimer(timerID);

    lanClientDlg = nullptr;
    delete ui;
}

void LANStartClientDialog::onGameSelectionChanged(const QItemSelection& cur, const QItemSelection& prev)
{
    QModelIndexList indlist = cur.indexes();
    if (indlist.count() == 0)
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

void LANStartClientDialog::on_tvAvailableGames_doubleClicked(QModelIndex index)
{
    done(QDialog::Accepted);
}

void LANStartClientDialog::onDirectConnect()
{
    if (ui->txtPlayerName->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "melonDS", "请在连接前输入玩家名称。");
        return;
    }

    QString host = QInputDialog::getText(this, "直接连接", "主机端地址:");
    if (host.isEmpty()) return;

    std::string hostname = host.toStdString();
    std::string player = ui->txtPlayerName->text().toStdString();

    setEnabled(false);
    lan().EndDiscovery();
    if (!lan().StartClient(player.c_str(), hostname.c_str()))
    {
        QString msg = QString("连接到主机端失败 %0。").arg(QString::fromStdString(hostname));
        QMessageBox::warning(this, "melonDS", msg);
        setEnabled(true);
        lan().StartDiscovery();
        return;
    }

    setEnabled(true);
    lanDlg = LANDialog::openDlg(parentWidget());
    QDialog::done(QDialog::Accepted);
}

void LANStartClientDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    if (r == QDialog::Accepted)
    {
        if (ui->txtPlayerName->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "melonDS", "请在连接前输入玩家名称。");
            return;
        }

        QModelIndexList indlist = ui->tvAvailableGames->selectionModel()->selectedRows();
        if (indlist.count() == 0) return;

        QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
        QStandardItem* item = model->item(indlist[0].row());
        u32 addr = item->data().toUInt();
        char hostname[16];
        snprintf(hostname, 16, "%d.%d.%d.%d", (addr>>24), ((addr>>16)&0xFF), ((addr>>8)&0xFF), (addr&0xFF));

        std::string player = ui->txtPlayerName->text().toStdString();

        setEnabled(false);
        lan().EndDiscovery();
        if (!lan().StartClient(player.c_str(), hostname))
        {
            QString msg = QString("连接到主机端失败 %0。").arg(QString(hostname));
            QMessageBox::warning(this, "melonDS", msg);
            setEnabled(true);
            lan().StartDiscovery();
            return;
        }

        setEnabled(true);
        lanDlg = LANDialog::openDlg(parentWidget());

        auto cfg = Config::GetGlobalTable();
        cfg.SetString("LAN.PlayerName", player);
        Config::Save();
    }
    else
    {
        lan().EndDiscovery();
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}

void LANStartClientDialog::timerEvent(QTimerEvent *event)
{
    doUpdateDiscoveryList();
}

void LANStartClientDialog::doUpdateDiscoveryList()
{
    auto disclist = lan().GetDiscoveryList();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
    int curcount = model->rowCount();
    int newcount = disclist.size();
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    int i = 0;
    for (const auto& [key, data] : disclist)
    {
        model->item(i, 0)->setText(data.SessionName);
        model->item(i, 0)->setData(QVariant(key));

        QString plcount = QString("%0/%1").arg(data.NumPlayers).arg(data.MaxPlayers);
        model->item(i, 1)->setText(plcount);

        QString status;
        switch (data.Status)
        {
            case 0: status = "空闲(Idle)"; break;
            case 1: status = "运行中(Playing)"; break;
        }
        model->item(i, 2)->setText(status);

        QString ip = QString("%0.%1.%2.%3").arg(key>>24).arg((key>>16)&0xFF).arg((key>>8)&0xFF).arg(key&0xFF);
        model->item(i, 3)->setText(ip);

        i++;
    }
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);
    const QStringList header = {"#", "玩家", "状态", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    timerID = startTimer(1000);
}

LANDialog::~LANDialog()
{
    killTimer(timerID);

    delete ui;
}

void LANDialog::on_btnLeaveGame_clicked()
{
    done(QDialog::Accepted);
}

void LANDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    bool showwarning = true;
    if (lan().GetNumPlayers() < 2)
        showwarning = false;

    if (showwarning)
    {
        if (QMessageBox::warning(this, "melonDS", "确定离开该局域网(LAN)游戏?",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            return;
    }

    lan().EndSession();
    setMPInterface(MPInterface_Local);

    QDialog::done(r);
}

void LANDialog::timerEvent(QTimerEvent *event)
{
    doUpdatePlayerList();
}

void LANDialog::doUpdatePlayerList()
{
    auto playerlist = lan().GetPlayerList();
    auto maxplayers = lan().GetMaxPlayers();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();
    int curcount = model->rowCount();
    int newcount = playerlist.size();
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    int i = 0;
    for (const auto& player : playerlist)
    {
        QString id = QString("%0/%1").arg(player.ID+1).arg(maxplayers);
        model->item(i, 0)->setText(id);

        QString name = player.Name;
        model->item(i, 1)->setText(name);

        QString status = "???";
        switch (player.Status)
        {
            case LAN::Player_Client:
                status = "已连接(Connected)";
                break;
            case LAN::Player_Host:
                status = "游戏主机端(Game host)";
                break;
            case LAN::Player_Connecting:
                status = "连接中(Connecting)";
                break;
            case LAN::Player_Disconnected:
                status = "连接丢失(Connection lost)";
                break;
            case LAN::Player_None:
                break;
        }
        model->item(i, 2)->setText(status);

        if (player.IsLocalPlayer)
        {
            model->item(i, 3)->setText("-");
            model->item(i, 4)->setText("(本地)");
        }
        else
        {
            if (player.Status == LAN::Player_Client ||
                player.Status == LAN::Player_Host)
            {
                QString ping = QString("%0 ms").arg(player.Ping);
                model->item(i, 3)->setText(ping);
            }
            else
            {
                model->item(i, 3)->setText("-");
            }


            u32 ip = player.Address;

            QString ips = QString("%0.%1.%2.%3").arg(ip&0xFF).arg((ip>>8)&0xFF).arg((ip>>16)&0xFF).arg(ip>>24);
            model->item(i, 4)->setText(ips);
        }

        i++;
    }
}
