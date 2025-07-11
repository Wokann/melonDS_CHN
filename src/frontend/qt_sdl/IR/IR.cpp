
#include <QTcpServer>
#include <QTcpSocket>
#include <QtSerialPort/QSerialPort>



#include "Platform.h"
#include "Config.h"
#include "EmuInstance.h"

#include "PokeEscalator.h"
//Improvements to be made.

namespace melonDS::Platform
{

int IRMode = 0;
//---------------------TCP = 2
QTcpServer *server = nullptr;
QTcpSocket *sock = nullptr;
void IR_OpenTCP(void * userdata){

    int conn = 0;
    if (!server){

        server = new QTcpServer();
        if (!server->listen(QHostAddress::Any, 8081)) {
            printf("Failed to start TCP server: %s\n", server->errorString().toUtf8().constData());
            return;
        }
        printf("TCP server listening on port 8081\n");
    }

    else return;

    while(conn == 0){
        QCoreApplication::processEvents();
        if (!sock && server->hasPendingConnections()) {
            sock = server->nextPendingConnection();
            printf("Client connected\n");
            conn = 1;
        }
        QThread::msleep(10); // avoid CPU spin
    }
}

u8 IR_TCP_SendPacket(char* data, int len, void * userdata){
    IR_OpenTCP(userdata);
    QCoreApplication::processEvents();

    if (!sock || sock->state() != QAbstractSocket::ConnectedState){
        printf("No client connected to send IR data\n");
        return 0;
    }

    qint64 written = sock->write(data, len);
    sock->flush();

    printf("Sent %lld bytes to client\n", written);
    return 0;
}


u8 IR_TCP_RecievePacket(char* data, int len, void * userdata){
    IR_OpenTCP(userdata);
    QCoreApplication::processEvents();
    if (!sock || sock->bytesAvailable() <= 0){
        return 0;
    }
    qint64 bytesRead = sock->read(data, len); //len is maxLen

    if (bytesRead > 0){
        return static_cast<int>(bytesRead);
    }
    printf("Read 0 bytes\n");
    return 0;
}



//--------------------SERIAL = 1
QSerialPort *serial = nullptr;
void IR_OpenSerialPort(void * userdata){
    if (!serial){

        EmuInstance* inst = (EmuInstance*)userdata;
        auto& cfg = inst->getLocalConfig();




        serial = new QSerialPort();
        serial->setPortName(cfg.GetQString("IR.SerialPortPath"));
        serial->setBaudRate(QSerialPort::Baud115200);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);
        if (!serial->open(QIODevice::ReadWrite)) {
            printf("Failed to open serial port %s\n", serial->errorString().toUtf8().constData());
        }
        else printf("Serial port open:\n");
    }
    else return;
}
u8 IR_Serial_SendPacket(char* data, int len, void * userdata){
    IR_OpenSerialPort(userdata);
    QCoreApplication::processEvents(); // allow Qt to update I/O status
    if (!serial || !serial->isOpen()) {
        printf("Serial write failed: port not open\n");
        return 0;
    }
    qint64 written = serial->write(data, len);
    serial->flush();
    printf("Serial wrote %lld bytes\n", written);
    return static_cast<u8>(written);

}
u8 IR_Serial_RecievePacket(char* data, int len,void * userdata){
    IR_OpenSerialPort(userdata);
    QCoreApplication::processEvents(); // allow Qt to update I/O status
    if (!serial || !serial->isOpen() || !serial->bytesAvailable()) {
        return 0;
    }
    qint64 bytesRead = serial->read(data, len);
    if (bytesRead > 0) {
        printf("Serial Read %lld bytes: ", bytesRead);
        for (int i = 0; i < bytesRead; ++i)
            printf("%02X ", static_cast<unsigned char>(data[i]));
        printf("\n");
        return static_cast<u8>(bytesRead);
    }
    return 0;
}

//Direct


int init = 0;
FileHandle * eh = nullptr;


char directRxBuffer[0xb8];
uint16_t directRxLength;

u8 IR_Direct_SendPacket(char* data, int len, void * userdata){
    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();




    directRxLength = txToWalker((FILE *) eh, len, data, directRxBuffer);


    //for (int i = 0; i < len; i++) printf("%02x ", (uint8_t) data[i]);

    if (len > 1) init = 99;
    return 0;
}
int on = 0;
u8 IR_Direct_RecievePacket(char* data, int len,void * userdata){
    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();



    //Let this be the detection of first comm
    if (!eh) eh = OpenFile(cfg.GetString("IR.EEPROMPath"), FileMode::ReadWriteExisting);

    char rtnLen = 0;

    //GAME STILL NEEDS TO INIT TO WALKER. SEND 0 BYTES TO WALKER, null list, return data buffer
    if (init < 10){

        init++;
        return 0;
    }
    else if (on == 0){
        on = 1;
        printf("Starting Walker init sequence\n");
        rtnLen = txToWalker((FILE *) eh, 0, data, data);
    }

    // GAME AT THIS POINT WILL HAVE SENT A PACKET. READ FROM BUFFER
    if (directRxLength != 0){


        for (int i = 0; i < directRxLength; i++) data[i] = (char) directRxBuffer[i];
        //memcpy(data, directRxBuffer, directRxLength);

        //for (int i = 0; i < directRxLength; i++ ) fprintf(stderr, "%02x ", (uint8_t) directRxBuffer[i] ^ 0xaa);


        /*rtnLen = directRxLength;


        directRxLength = 0;
*/

        return directRxLength;
    }
    //printf("Recieved packet of len: %d data[0]: %02x\n", rtnLen, data[0]);

    return rtnLen;
}







//--------------------------------Global
u8 IR_SendPacket(char* data, int len, void * userdata){
    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();


    IRMode = cfg.GetInt("IR.Mode");
    //printf("Trying to send IR Packet in mode: %d\n", IRMode);

    if (IRMode == 0) return 0;
    if (IRMode == 1) return IR_Serial_SendPacket(data, len, userdata);
    if (IRMode == 2) return IR_TCP_SendPacket(data, len, userdata);
    if (IRMode == 3) return IR_Direct_SendPacket(data,len,userdata);
    return 0;
}
u8 IR_RecievePacket(char* data, int len, void * userdata){
    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();

    IRMode = cfg.GetInt("IR.Mode");
    //printf("Trying to recieve IR Packet in mode: %d\n", IRMode);

    if (IRMode == 0) return 0;
    if (IRMode == 1) return IR_Serial_RecievePacket(data, len, userdata);
    if (IRMode == 2) return IR_TCP_RecievePacket(data, len, userdata);
    if (IRMode == 3) return IR_Direct_RecievePacket(data, len, userdata);
    return 0;
}


FileHandle * fh = nullptr;
//= OpenFile(cfg.GetString("IR.PacketLogFile"), FileMode::Append);
void IR_LogPacket(char * data, int len, bool isTx, void * userdata){


    char key = 0xAA;
    //printf("Trying to log a packet. Len: %d IsTx: %d \n", len, isTx);

    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();


    if (!fh) fh = OpenFile(cfg.GetString("IR.PacketLogFile"), FileMode::Append);


    if (isTx) FileWrite("Tx: ", 1, 4, fh);
    else FileWrite("Rx: ", 1, 4, fh);


    for (size_t i = 0; i < len; i++){
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", static_cast<unsigned char>(data[i] ^ key));
        FileWrite(hex, 1, strlen(hex), fh);
    }

    FileWrite("\n", 1, 1, fh);
    fflush((FILE *)fh);

}


bool IR_BypassDelay(void * userdata){
    EmuInstance* inst = (EmuInstance*)userdata;
    auto& cfg = inst->getLocalConfig();

    IRMode = cfg.GetInt("IR.Mode");
    if (IRMode == 3) return true;
    return false;

}




}
