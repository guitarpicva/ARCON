// Copyright 2026 GrizzWorks, LLC -- All Rights Reserved
#ifndef ARCON_H
#define ARCON_H

#include <QObject>
#include <QSerialPort>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class ARCON : public QObject
{
    Q_OBJECT
public:
    explicit ARCON(QObject *parent = nullptr, QString radioFilename="", bool startServer = false);

signals:
    void radioDataIn(); // data has arrived in [inbytes] from the radio connection (CI-V/CAT, etc.)
    void socketDisconnected();
    void socketConnected(const QString addr, const int port);

public slots:
    void startOver(const QString radioFilename); // start from scratch with this file
    void startARCONServer(); // create the QTcpServer instance
    void sendSetupLines();
    void sendTxsLines();
    void sendRxsLines();
    void sendPttOn();
    void sendPttOff();
    // For radios like Yaesu FT-450D and others which switch audio input
    // based on a special data ptt command.
    void sendPttData();
    void sendSplitOn();
    void sendSplitOff();
    void sendSplitToggle();
    // using the modeList map allows to set ANY user defined mode by name/value
    // pair in the JSON definition
    void sendSetMode(const QString modeName);
    void sendSetVFOFreq(QString freqHz, int vfo = 0);
    void sendToVFOA();
    void sendToVFOB();
    void sendAutoTune();
    void handleClientCommands(const QStringList cmds);
private slots:
    void onSocketStateChanged(QAbstractSocket::SocketState);
    void onSocketConnected();
    void onReadyRead();
    void sendRadioCommand(const QByteArray cmd);
    void sendCmd();
    void newConnection();
    void onClientReadyRead();
    void processClientData();
    void queryCmd(const QString cmd);
    void onRadioDataIn();
    QString getCIVFreq(const QString freqHz);
    QString getCATFreq(const QString freqHz);
    QString getBCDFreq(const QString freqHz);
private:
    bool isTcp = false;
    bool isBinary = false;
    QString pttMethod = "SERIAL"; // SERIAL or TCP (network) control connection
    QString radioAddress = "";
    QVector<QString> cmdLines; // list of current commands to send, may be 1 or more
    QVector<QString> setupLines; // ordered list of setup commands

    // next two may not be needed... AFDM/NGTerm specific terminology
    // CURRENTLY, not included in the json definitions, but kept for
    // posterity and possible use later.  This is an odd functionality
    // because it is meant to be done before PTT on (txs) and after
    // PTT off (rxs).  May only apply to a very small subset of radios.
    QVector<QString> txsLines; // ordered list of transmit commands
    QVector<QString> rxsLines; // ordered list of receive commands
    // END next two may not be needed
    const QByteArray clientHelpText = "ARCON Client Help\n\n \
Command List:\nptton\npttoff\npttdata\nvfoafreq\nvfobfreq\nspliton\nsplitoff\ntovfoa\ntovfob\nautotune\n \
Query Commands:\n?mode\n?vfo\n?vfoa\n?vfob\n";
    QString pttOn;
    QString pttOff;
    QString pttData;
    QString splitOn;
    QString splitOff;
    QString splitToggle;
    QString toVFOA;
    QString toVFOB;
    QString autoTune;
    QString modeQuery; // which mode is in use -- compare reply to modeList values
    QString vfoQuery; // which vfo is in use
    QString vfoAFreq; // query VFO A frequency
    QString vfoBFreq; // query VFO B frequency
    QString vfoAResponse; // expected response for VFO query where curr vfo is A
    QString vfoBResponse; // expected response for VFO B query where curr vfo is B
    QString vfoAFreqResponse; // expected respoinse for VFO A frequency query
    QString vfoBFreqResponse; // expected respoinse for VFO B frequency query
    QString modeResponsePrefix; // expected prefix of mode query reply // MDO for test only
    QMap<QString,QString> modeList;
    QByteArray inbytes; // incoming radio control data on serial or tcp
    QByteArray clientbytes; // incoming client socket data
    QString radioFile = ""; // the radio definitions file supplied by the user
    QString radioName ="?";
    QString freqPrefixA = "";
    QString freqPrefixB = "";
    QString freqSuffix = "";
    int freqLength = 8;
    QString freqType; // CI-V, CAT (ASCII), BCD (which uses ASCII function)
    int radioTcpPortNumber = 23; // CODAN default control
    QSerialPort *serial = nullptr;
    QTcpSocket *socket = nullptr;
    QTcpServer *server = nullptr;
    QTcpSocket *client = nullptr;
    bool b_startServer = false;
    bool b_cmdDebug = false;
    // void loadSettings(); // kept as a placeholder
    void startControlConnection();
    int serialBaudRate = 115200;
    int txTail = 20; // used at least by sendPttOff() function to hold PTT for set # ms before turning it off
    QString serialParams = "8N1";
    QString serialFlowControl = "No Flow Control";
    void loadRadioFile();
    const QString SPACE = " ";
    const QByteArray CRLF = "\r\n";
    QTimer cmdTimer; // used to send multiple commands with delay in between
    int cmdTimerInterval = 75; // initial command interval
    QTimer portTimer;
    int portTimerTimeout = 40; // ms until process the incoming data
    QTimer clientTimer; // used to wait on data from the client socket
    int clientTimerTimeout = 20; // ms until process client tcp data
    // setters
    void setRadioAddress(const QString portname) { radioAddress = portname; } // serial port name or IP address if TCP
    void setRadioFile(const QString radiofile) { radioFile = radiofile; }
    void setTcpPort(const int port) { radioTcpPortNumber = port; }
    void setIsTcp(bool istcp) { isTcp = istcp; }
    void setBaudRate(const int baudrate) { serialBaudRate = baudrate; }
    QString buildCIVFreq(const QString freq, const int length, int vfo = 0);
    QString buildCATFreq(const QString freq, const int length, int vfo = 0);
    QString buildBCDFreq(const QString freq, const int length = 8, const QString suffix = "01");
};

#endif // ARCON_H
