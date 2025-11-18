/*
    Copyright 2016-2024 melonDS team

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

    @BarretKlics
*/

#include <stdio.h>
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

#include "IRSettingsDialog.h"
#include "ui_IRSettingsDialog.h"

#include <QFileDialog>
IRSettingsDialog* IRSettingsDialog::currentDlg = nullptr;

bool IRSettingsDialog::needsReset = false;

void NetInit();
using namespace melonDS::Platform;
using namespace melonDS;

IRSettingsDialog::IRSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::IRSettingsDialog)
{


    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);


    //I am not a front end dev sorry about this :)
    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getLocalConfig();


    IRMode = cfg.GetInt("IR.Mode");

    lastDevIRFolder = cfg.GetQString("IR.LastDevIRFolder");






    connect(ui->rbCompat, &QRadioButton::toggled, this, &IRSettingsDialog::toggleCompatSettings);
    connect(ui->rbSerial, &QRadioButton::toggled, this, &IRSettingsDialog::toggleSerialSettings);
    connect(ui->rbTCP, &QRadioButton::toggled, this, &IRSettingsDialog::toggleTCPSettings);
    connect(ui->rbDirect, &QRadioButton::toggled, this, &IRSettingsDialog::toggleDirectSettings);


    connect(ui->rb_TCPServer, &QRadioButton::toggled, this, &IRSettingsDialog::toggleTCPServer);
    connect(ui->rb_TCPClient, &QRadioButton::toggled, this, &IRSettingsDialog::toggleTCPClient);



    if (IRMode == 0) ui->rbCompat->setChecked(true);
    else if (IRMode == 1) ui->rbSerial->setChecked(true);
    else if (IRMode == 2) ui->rbTCP->setChecked(true);
    else if (IRMode == 3) ui->rbDirect->setChecked(true);



    ui->groupBoxSerial->setEnabled(false);
    ui->groupBoxTCP->setEnabled(false);
    ui->groupBoxDirect->setEnabled(false);




    ui->txtSerialPath->setText(cfg.GetQString("IR.SerialPortPath"));
    ui->txtEepromFile->setText(cfg.GetQString("IR.EEPROMPath"));
    //ui->textSerialPath->text());



    toggleCompatSettings(ui->rbCompat->isChecked());
    toggleSerialSettings(ui->rbSerial->isChecked());
    toggleTCPSettings(ui->rbTCP->isChecked());
    toggleDirectSettings(ui->rbDirect->isChecked());




    ui->lblSelfIP->setText(QString("000.000.0.0"));

    ui->txtDevLog->setText(cfg.GetQString("IR.PacketLogFile"));







}

void IRSettingsDialog::toggleCompatSettings(bool checked)
{
    //ui->groupBoxSerial->setEnabled(checked);

}
void IRSettingsDialog::toggleSerialSettings(bool checked)
{
    ui->groupBoxSerial->setEnabled(checked);

}

void IRSettingsDialog::toggleTCPSettings(bool checked){

    ui->groupBoxTCP->setEnabled(checked);
}

void IRSettingsDialog::toggleDirectSettings(bool checked){

    ui->groupBoxDirect->setEnabled(checked);
}





void IRSettingsDialog::toggleTCPServer(bool checked){

    //ui->groupBoxTCP->setEnabled(checked);
}

void IRSettingsDialog::toggleTCPClient(bool checked){

    //ui->groupBoxDirect->setEnabled(checked);
}




inline void IRSettingsDialog::updateLastDevIRFolder(QString& filename)
{
    int pos = filename.lastIndexOf("/");
    if (pos == -1)
    {
        pos = filename.lastIndexOf("\\");
    }

    QString path_dir = filename.left(pos);
    auto& cfg = emuInstance->getLocalConfig();
    cfg.SetQString("IR.LastDevIRFolder", path_dir);
    lastDevIRFolder = path_dir;
}




void IRSettingsDialog::on_PacketLogBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select IR log file",
                                                lastDevIRFolder,
                                                "Txt files (*.txt);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to IR packlet log file.\nPlease check file/folder write permissions.");
        return;
    }


    updateLastDevIRFolder(file);

    ui->txtDevLog->setText(file);
}


void IRSettingsDialog::on_EepromBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select pokewalker EEPROM file",
                                                "",
                                                "bin files (*.bin);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to EEPROM file.\nPlease check file/folder write permissions.");
        return;
    }


    ui->txtEepromFile->setText(file);
}












IRSettingsDialog::~IRSettingsDialog()
{
    delete ui;
}

void IRSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    needsReset = false;

    if (r == QDialog::Accepted)
    {

        if (ui->rbCompat->isChecked() == true) IRMode = 0;
        if (ui->rbSerial->isChecked() == true) IRMode = 1;
        if (ui->rbTCP->isChecked() == true) IRMode = 2;
        if (ui->rbDirect->isChecked() == true) IRMode = 3;
        //printf("IrMode: %d\n", IRMode);


        auto& cfg = emuInstance->getLocalConfig();

        cfg.SetInt("IR.Mode", IRMode);



        cfg.SetQString("IR.SerialPortPath", ui->txtSerialPath->text());

        cfg.SetQString("IR.EEPROMPath", ui->txtEepromFile->text());




      //  auto& cfg = emuInstance->getGlobalConfig();
      //  auto& instcfg = emuInstance->getLocalConfig();

       // cfg.SetBool("Emu.ExternalBIOSEnable", ui->chkExternalBIOS->isChecked());
        cfg.SetQString("IR.PacketLogFile", ui->txtDevLog->text());









        /*
        cfg.SetBool("LAN.IPMode", ui->rbIPMode->isChecked());

        int sel = ui->cbxIPAdapter->currentIndex();
        if (sel < 0 || sel >= adapters.size()) sel = 0;
        if (adapters.empty())
        {
            //cfg.SetString("LAN.Device", "");
        }
        else
        {
            //cfg.SetString("LAN.Device", adapters[sel].DeviceName);
        }
        */
        Config::Save();
    }

    //Config::Table cfg = Config::GetGlobalTable();
    //std::string devicename = cfg.GetString("LAN.Device");

    //NetInit();

    QDialog::done(r);

    closeDlg();
}



