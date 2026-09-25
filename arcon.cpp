// Copyright 2026 GrizzWorks, LLC -- All Rights Reserved
#include "arcon.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
//#include <QSettings>
#include <QThread>

#include <QDebug>

ARCON::ARCON(QObject *parent, QString radioFilename, bool startServer)
    : QObject{parent},
    radioFile(radioFilename),
    b_startServer(startServer)
{
    // Set up system for controlling radio here based on config data.
    // Currently has no implementation, but a placeholder.
    // loadSettings();
    // Load the specified radio RSON (JSON) file here to fill all of the
    // instance variables before starting the connection to the device
    // and before starting the TCP server for external clients.
    loadRadioFile();
    // Length of time (ms) that the port is quiet before processing data
    // which has been gathered in the [inbytes] instance buffer.
    portTimer.setInterval(portTimerTimeout);
    connect(&portTimer, &QTimer::timeout, this, &ARCON::onRadioDataIn);
    // Length of time (ms) between sending each of a list of commands to the device.
    cmdTimer.setInterval(cmdTimerInterval);
    connect(&cmdTimer, &QTimer::timeout, this, &ARCON::sendCmd);
    // Start the connection to the radio via serial or TCP socket.
    startControlConnection();
    // It will be possible (later) to turn off the TCP server socket
    // and use this code internally to a program instead.  For now
    // [b_startServer] is hard-coded to [true]
    if(b_startServer) {
        // qDebug()<<"start the client server";
        // Set up the client data timer timeout value, so we know when
        // all client data has arrived.
        clientTimer.setInterval(clientTimerTimeout);
        // WHen the [clientTimer] times out, process all of the collected data.
        connect(&clientTimer, &QTimer::timeout, this, &ARCON::processClientData);
        // Now start the TCP server socket.
        startARCONServer();
    }
}

// Instantiate the TCP server socket and bind it to port 19791
// and then await a client connection.
void ARCON::startARCONServer() {
    if(server) {
        server->close(); // stop all connections
        disconnect(server);
    }
    server = new QTcpServer(this);
    //qDebug()<<"server listening:"<<server->listen(QHostAddress::AnyIPv4, 19791);
    connect(server, &QTcpServer::newConnection, this, &ARCON::newConnection);
    // qDebug()<<"ARCON server: "<<server->serverAddress().toString()<<server->serverPort();
    //connect(server, &QTcpServer::acceptError, [=]{qDebug()<<"error";});
}

// A client has connected to the TCP server socket, so set
// up the connection and attache it to the socket's readyRead
// signal for data handling by [onClientReadyRead].
void ARCON::newConnection() {
    // only one socket connection allowed at a time
    if(client) {
        client->abort();
        disconnect(client);
    }
    client = server->nextPendingConnection();
    //qDebug()<<"Client connection from "<<client->peerAddress()<<":"<<client->peerPort();
    connect(client, &QTcpSocket::readyRead, this, &ARCON::onClientReadyRead, Qt::UniqueConnection);
}

// Collect incoming data to the TCP client socket
// and rely on the timer to trigger processClientData
// once it is has all arrived.
void ARCON::onClientReadyRead() {
    clientTimer.stop();
    clientbytes.append(client->readAll());
    qDebug()<<"onClientReadyRead:"<<clientbytes;
    clientTimer.start(); // when finally expires, run processClientData()
}

// Once all data has arrived on the TCP client socket, process them
// and clear the input buffer [clientbytes].
void ARCON::processClientData() {
    clientTimer.stop(); // while we are processing data, restarted by onClientReadyRead()
    // qDebug()<<"processClientData:"<<clientbytes;
    // here is where we would take the plain text commands, which match the public slots
    // and trigger them, and also pass back any indications that are relevant.
    // commands are delimited by a "|" character, if more than one.
    const QStringList cmds = QString(clientbytes).trimmed().toLower().split('|', Qt::SkipEmptyParts);
    clientbytes.clear();
    handleClientCommands(cmds);
}

// When a list of commands comes over the TCP client socket, handle each one.
void ARCON::handleClientCommands(const QStringList cmds) {
    foreach(const QString cmd, cmds) {
        qDebug()<<"processClientData cmd:"<<cmd;
        if(cmd.startsWith("?")) {
            // send to the query handler
            queryCmd(cmd.mid(1));
        }
        if(cmd == "ptton") {sendPttOn();}
        else if(cmd == "pttdata") {sendPttData();}
        else if(cmd == "pttoff") {sendPttOff();}
        else if(cmd.startsWith("vfoafreq")) {
            const QStringList parts = cmd.simplified().split(" ");
            qDebug()<<"Parts:"<<parts;
            if(parts.length() < 2) {return;}
            const QString freq = parts.at(1);
            if(freq.isEmpty()) {return;}
            sendSetVFOFreq(freq, 0);
        }
        else if(cmd.startsWith("vfobfreq")) {
            const QStringList parts = cmd.simplified().split(" ");
            qDebug()<<"Parts:"<<parts;
            if(parts.length() < 2) {return;}
            const QString freq = parts.at(1);
            if(freq.isEmpty()) {return;}
            sendSetVFOFreq(freq, 1);
        }
        else if(cmd.startsWith("mode")) {
            const QStringList parts = cmd.simplified().split(" ");
            if(parts.length() < 2) {return;}
            const QString mode = parts.at(1);
            if(!mode.isEmpty()) {
                qDebug()<<"sendSetMode"<<mode;
                sendSetMode(mode);
            }
            else {qDebug()<< "mode is empty...:(";}
        }
        else if(cmd == "tovfoa") {sendToVFOA();}
        else if(cmd == "tovfob") {sendToVFOB();}
        else if(cmd == "autotune") {sendAutoTune();}
        else if(cmd == "setuplines") {sendSetupLines();}
        else if(cmd == "spliton") {sendSplitOn();}
        else if(cmd == "splitoff") {sendSplitOff();}
        else if(cmd == "splittoggle") {sendSplitToggle();}
        else if(cmd == "txslines") {sendTxsLines();}
        else if(cmd == "rxslines") {sendRxsLines();}
        else if(cmd.startsWith("help")) {
            client->write(clientHelpText);
            client->flush();
        }
        else if(cmd.startsWith("restart")) {
            startOver(radioFile);
            client->write(("restarting " % radioFile.split("/").last() % CRLF).toUtf8());
            qDebug()<<"restart with current radio file"<<radioFile.split("/").last();
            return; // just in case things are added after this
        }
        else if(cmd.startsWith("debug")) {
            const QStringList parts = cmd.simplified().toLower().split(" ");
            if(parts.length() > 1) {
                b_cmdDebug = parts.at(1) == "on";
                client->write("cmd debug " + parts.at(1).toUtf8() + CRLF);
            }
        }
    }
}

// Handle query commands from the end user which have arrived
// over the TCP socket, and prefixed by a "?".
void ARCON::queryCmd(const QString cmd) {
    qDebug()<<"Query CMD:"<<cmd;
    QString out;
    // decide which query cmd to use based on the API command passed
    if(cmd.trimmed().isEmpty()) { return; }
    out = cmd.toUpper(); // TODO: TEMPORARY TEST for raw commands
    if(cmd.startsWith("vfoa")) {
        out = vfoAFreq;
    }
    else if(cmd.startsWith("vfob")) {
        out = vfoBFreq;
    }
    else if(cmd.startsWith("vfo")) {
        out = vfoQuery;
    }
    else if(cmd.startsWith("mode")) {
        out = modeQuery;
        qDebug()<<"mode query:"<<out;
    }
    cmdLines = out.split(",", Qt::SkipEmptyParts); // skip empty parts
    qDebug()<<"queryCommands:"<<cmdLines<<cmdLines.length();
    if(cmdLines.length() > 0) { cmdTimer.start(); }
}

// Send the first command in the list of commands [cmdLines] and
// then remove it from the list of commands.  A timer determines
// the inter-command timing based on the [commandTimeout] variable..
void ARCON::sendCmd() {
    /// Send first command in the QVector<QString> (ordered list),
    /// then delete it from the list.  This is a slot for a
    /// QTimer::timeout signal.
    qDebug()<<"sendCmd cmdLines:"<<cmdLines;
    if(cmdLines.length() > 0) {
        cmdTimer.stop();
        const QByteArray cmd = cmdLines.at(0).toUtf8();
        if(cmd.trimmed().isEmpty()) { cmdLines.removeFirst();  return; }
        sendRadioCommand((freqType == "CAT") ? cmd: QByteArray::fromHex(cmd));
        cmdLines.removeFirst();
        if(cmdLines.isEmpty()) { cmdTimer.stop(); }
        else { cmdTimer.start(); }
        qDebug()<<"sendRadioCommand:"<<cmd;
    }
    else { cmdTimer.stop(); } // failsafe to kill timer if empty
}

/// Read a new RadioFiles JSON file and start everything over again.
void ARCON::startOver(const QString radioFilename){
    if(!radioFilename.trimmed().isEmpty()) {
        // use the new file, otherwise keep the original one
        radioFile = radioFilename;
    }
    //loadSettings(); // kept as a placeholder in case needed later
    loadRadioFile();
    startControlConnection();
    // TCP server is already started if configured, so leave
    // that alone.
}

// This may go away unless there is a good need for it.
// Radio settings json file should contain everything needed.
// void ARCON::loadSettings() {
//     //qDebug()<<"Load Radio Control Settings: dummy";
// }

// Read the RSON radio file and load all of the instance variables
// in order to control the radio.
void ARCON::loadRadioFile() {
    // qDebug()<<"Load Radio File:"<<radioFile;
    QFile f(radioFile);
    if(f.open(QFile::ReadOnly)) {
        QByteArray json;
        json = f.readAll();
        f.close();
        // qDebug().noquote()<<"json raw:"<<json;
        QJsonDocument jd = QJsonDocument::fromJson(json);
        // /qDebug()<<"json ok?"<<jd.isObject();
        if(jd.isObject()) {
            QJsonObject jo = jd.object();
            // now we can walk the object to load the UI
            // radio tab
            radioName = jo.value("radioName").toString();
            // this now set by freqType below
            // isBinary = jo.value("controlType").toString() == "BINARY";
            isTcp = jo.value("connectionMethod").toString() == "SERIAL" ? false : true;
            radioAddress = jo.value("radioAddress").toString();
            serialBaudRate = jo.value("serialBaudRate").toInt(19200);
            serialParams = jo.value("serialParams").toString();
            serialFlowControl = jo.value("serialFlowControl").toString();
            radioTcpPortNumber = jo.value("radioTcpPortNumber").toInt(23);
            portTimerTimeout = jo.value("portTimeout").toInt(40);
            cmdTimerInterval = jo.value("commandTimeout").toInt(40);
            // setup tab
            QStringList sltmp = jo.value("initialSetup").toString().split(",");
            foreach(const QString s, sltmp) {
                if(s.trimmed().length() > 0) {
                    setupLines<<s;
                }
            }
            pttOn = jo.value("pttOn").toString();
            pttOff = jo.value("pttOff").toString();
            pttData = jo.value("pttData").toString();
            txTail = jo.value("txTail").toInt(20); // def. 20 ms for bad/missing value
            splitOn = jo.value("splitOn").toString();
            splitOff = jo.value("splitOff").toString();
            toVFOA = jo.value("toVFOA").toString();
            toVFOB = jo.value("toVFOB").toString();
            autoTune = jo.value("autoTune").toString();
            // modes list
            QJsonObject jtmp = jo.value("modeList").toObject();
            QStringList keys = jtmp.keys();
            foreach(const QString key, keys) {
                modeList.insert(key, jtmp.value(key).toString());
            }
            jtmp = jo.value("frequencyControl").toObject();
            if(!jtmp.isEmpty()) {
                freqType = jtmp.value("order").toString();
                // decide on binary or ascii based on freqType setting
                isBinary = freqType != "CAT";
                freqLength = jtmp.value("numDigits").toInt();
                freqPrefixA = jtmp.value("prefixA").toString();
                freqPrefixB = jtmp.value("prefixB").toString();
                freqSuffix = jtmp.value("suffix").toString();
            }
            jtmp = jo.value("queryCommands").toObject();
            if(!jtmp.isEmpty()) {
                // load the query commands
                modeQuery = jtmp.value("mode").toString();
                modeResponsePrefix = jtmp.value("modeResponsePrefix").toString();
                vfoQuery = jtmp.value("vfo").toString();
                vfoAResponse = jtmp.value("vfoAResponse").toString();
                vfoBResponse = jtmp.value("vfoBResponse").toString();
                vfoAFreqResponse = jtmp.value("vfoAFreqResponse").toString();
                vfoBFreqResponse = jtmp.value("vfoBFreqResponse").toString();
                vfoAFreq = jtmp.value("vfoAFreq").toString();
                vfoBFreq = jtmp.value("vfoBFreq").toString();
            }
            // may wish to add ad-hoc mode list here later, from a new
            // section of the JSON maybe called userModeList {}
        }
        // qDebug()<<"Radio Name:"<<radioName;
        // qDebug()<<"Binary Radio?"<<isBinary;
        // qDebug()<<"setup:"<<setupLines;
        // qDebug()<<"txs:"<<txsLines;
        // qDebug()<<"rxs:"<<rxsLines;
        // qDebug()<<"pttOn:"<<pttOn;
        // qDebug()<<"pttOff:"<<pttOff;
        // qDebug()<<"txTail:"<<txTail;
        // qDebug()<<"splitOn:"<<splitOn;
        // qDebug()<<"splitOff:"<<splitOff;
        // qDebug()<<"radioTcpPortNumber:"<<radioTcpPortNumber;
        // qDebug()<<"freqType:"<<freqType;
        // qDebug()<<"freqLength:"<<freqLength;
        // qDebug()<<"freqPrefix:"<<freqPrefixA<<freqPrefixB;
        // qDebug()<<"freqSuffix:"<<freqSuffix;
        // qDebug()<<"modeList:"<<modeList;
        // qDebug()<<"modeQuery:"<<modeQuery;
        // qDebug()<<"modeResponsePrefix:"<<modeResponsePrefix;
        // qDebug()<<"vfoQuery:"<<vfoQuery;
        // qDebug()<<"vfoAResponse:"<<vfoAResponse;
        // qDebug()<<"vfoBResponse:"<<vfoBResponse;
        // qDebug()<<"vfoAFreq:"<<vfoAFreq;
        // qDebug()<<"vfoBFreq:"<<vfoBFreq;
    }
}

// Once the radio connection has been accomplished, send any setup
// commands required for the radio.  Set which VFO, freq, mode, filter
// width, or any other commands the user has chosen.
void ARCON::sendSetupLines() {
    // spaces automatically skipped in QByteArray::fromhex() function
    qDebug()<<"Send setup lines"<<setupLines;
    cmdLines = setupLines;
    cmdTimer.setInterval(cmdTimerInterval);
    cmdTimer.start(cmdTimerInterval);
}

// CURRENTLY UNUSED: send a list of commands as required before
// turning ON the PTT to put the radio into proper transmit state.
void ARCON::sendTxsLines() {
    qDebug()<<"Send txs lines"<<txsLines;
    cmdLines = txsLines;
    cmdTimer.setInterval(cmdTimerInterval);
    cmdTimer.start(cmdTimerInterval);
}

// CURRENTLY UNUSED: send a list of commands as required after
// turning OFF the PTT to put the radio into proper receive state.
void ARCON::sendRxsLines() {
    qDebug()<<"Send rxs lines";
    cmdLines = rxsLines;
    cmdTimer.setInterval(cmdTimerInterval);
    cmdTimer.start(cmdTimerInterval);
}

// Send the radio command to turn on PTT
void ARCON::sendPttOn() {
    qDebug()<<"RC: sendPttOn"<<pttOn;
    // TX Setup Lines are used before EVERY PTT ON
    if(txsLines.length() > 0) { sendTxsLines(); } // this has QThread::wait() calls so a bit slow to return...
    QTimer::singleShot(20, [=] {
        sendRadioCommand(isBinary?QByteArray::fromHex(pttOn.toUtf8()):pttOn.toUtf8());
    });
}

// Send the radio command to turn off PTT
void ARCON::sendPttOff() {
    // qDebug()<<"RC: sendPttOff"<<"isBinary?"<<isBinary<<pttOff;
    QTimer::singleShot(txTail, [=] { // keep PTT up until txTail expires
        sendRadioCommand(isBinary ? QByteArray::fromHex(pttOff.toUtf8()) : pttOff.toUtf8());
    });
    // RX Setup Lines are used after EVERY PTT OFF
    if(rxsLines.length() > 0) { sendRxsLines(); } // this has QThread::wait() calls so a bit slow to return...
}

// If possible, send the command to turn on split vfo operation.
void ARCON::sendSplitOn()
{
    qDebug()<<"Send Split ON"<<splitOn;
    sendRadioCommand(isBinary ? QByteArray::fromHex(splitOn.toUtf8()) : splitOn.toUtf8());
}

// If possible, send the command to turn off split vfo operation.
void ARCON::sendSplitOff()
{
    qDebug()<<"Send Split OFF"<<splitOff;
    sendRadioCommand(isBinary ? QByteArray::fromHex(splitOff.toUtf8()) : splitOff.toUtf8());
}

// If possible, send the command to toggle the split VFO state.
void ARCON::sendSplitToggle()
{
    qDebug()<<"Send Split Toggle"<<splitToggle;
    sendRadioCommand(isBinary ? QByteArray::fromHex(splitToggle.toUtf8()) : splitToggle.toUtf8());
}

// If possible send the PTT data command to the radio.
void ARCON::sendPttData()
{
    qDebug()<<"RC: sendPttData";
    QTimer::singleShot(20, [=] {
        sendRadioCommand(isBinary ? QByteArray::fromHex(pttData.toUtf8()) : pttData.toUtf8());
    });
}

// Set the radio mode by name correlated by the [modeList] QMap
void ARCON::sendSetMode(const QString modeName)
{
    qDebug()<<"sendSetMode:"<<modeName;
    const QString modelist = modeList.value(modeName.toUpper());
    // if(modeList.contains(modeName.toUpper())) {
    //     sendRadioCommand(isBinary ? QByteArray::fromHex(modeList.value(modeName.toUpper()).toUtf8()) : modeList.value(modeName.toUpper()).toUtf8());
    // }
    const QStringList commands = modelist.split(",", Qt::SkipEmptyParts);
    cmdLines = commands;
    cmdTimer.setInterval(cmdTimerInterval);
    cmdTimer.start(cmdTimerInterval);
}

// Set the VFO frequency
void ARCON::sendSetVFOFreq(QString freqHz, int vfo)
{
    // qDebug()<<"sendSetVFOFreq:"<<freqHz<<"vfo:"<<vfo;
    if(isBinary) { // CI-V or BCD
        qDebug()<<"setVFO freq type:"<<freqType;
        const QByteArray pre = vfo == 0 ? freqPrefixA.toUtf8() : freqPrefixB.toUtf8();
        QByteArray freq;
        if(freqType == "CI-V") { freq = buildCIVFreq(freqHz, freqLength, vfo).toUtf8(); }
        else if(freqType == "BCD") {
            while(freqHz.length() < 8) { freqHz.prepend("0"); }
            freq = QByteArray::fromHex((freqHz % "01").toUtf8());
        }
        sendRadioCommand(QByteArray::fromHex(freq));
        qDebug()<<"BCD or CI-V Freq:"<<freq;
    }
    else {
        // ASCII CAT and friends
        const QString pre = vfo == 0 ? freqPrefixA : freqPrefixB;
        const QString catfreq = buildCATFreq(freqHz, freqLength, vfo);
        // I DON'T LIKE THIS ONE OFF HANDLING FOR STUPID CODAN BUT IT IS WHAT IT IS
        // ALL OTHER RADIOS SET FREQUENCY in HZ EXCEPT CODAN WHERE THE HZ PARAMS ONLY
        // WORK FOR THIS ONE COMMAND "CONNECT TCVR RF <FREQ> <FREQ>".
        if(radioName.startsWith("CODAN")) {
            // add a second frequency in Hz to the end after a space to placate Oz
            // and add the CODAN line terminator for CICS of a carriage return.
            sendRadioCommand(catfreq.trimmed().toUtf8() + " " + freqHz.toUtf8() + "\r");
        }
        else {
            sendRadioCommand(catfreq.toUtf8());
        }
        qDebug()<<"CAT Freq:"<<catfreq.trimmed();
    }
}

// If possible, move the radio to VFO A
void ARCON::sendToVFOA()
{
    if(freqType == "CAT") {
        sendRadioCommand(toVFOA.toUtf8());
    }
    else {
        // else binary CI-V or BCD
        qDebug()<<"send tovfob CI-V"<<toVFOA;
        sendRadioCommand(QByteArray::fromHex(toVFOA.toUtf8()));
    }
}

// If possible, move the radio to VFO B
void ARCON::sendToVFOB()
{
    if(freqType == "CAT") {
        // ASCII CAT
        sendRadioCommand(toVFOB.toUtf8());
    }
    else {
        // CI-V or BCD
        sendRadioCommand(QByteArray::fromHex(toVFOB.toUtf8()));
    }
}

// Send the configured auto-tune command to the radio.
void ARCON::sendAutoTune()
{
    if(freqType == "CAT") {
        sendRadioCommand(autoTune.toUtf8());
    }
    else {
        sendRadioCommand(QByteArray::fromHex(autoTune.toUtf8()));
    }
}

// Build the control connection to the radio as either serial or TCP socket.
void ARCON::startControlConnection() {
    if(isTcp) {
        qDebug()<<"Tcp Connection Started";
        if(serial) { serial->close(); } // could have been serial before
        if(socket) {
            socket->abort();
        }
        socket = new QTcpSocket(this);
        connect(socket, &QTcpSocket::stateChanged, this, &ARCON::onSocketStateChanged, Qt::UniqueConnection);
        connect(socket, &QTcpSocket::readyRead, this, &ARCON::onReadyRead, Qt::UniqueConnection);
        socket->connectToHost(radioAddress, radioTcpPortNumber);
        // socket->waitForConnected(5000);
        // setup lines sent when socket reports connected in onSocketStateChanged
    }
    else {
        //qDebug()<<"Serial Connection Started";
        if(socket) { socket->abort(); } // could have been socket before
        if(serial) {
            serial->close();
        }
        serial = new QSerialPort(radioAddress, this);
        connect(serial, &QSerialPort::errorOccurred, this, [=](int err){
            qDebug()<<"Serial Port Error:"<<err;
        });

        if(serial->open(QSerialPort::ReadWrite)) {
            serial->setBaudRate(serialBaudRate);
            serial->setParity(QSerialPort::NoParity);
            serial->setDataBits(QSerialPort::Data8);
            serial->setStopBits(QSerialPort::OneStop);
            serial->setFlowControl(QSerialPort::NoFlowControl);
            qDebug()<<"Serial Port Open!";
            connect(serial, &QSerialPort::readyRead, this, &ARCON::onReadyRead, Qt::UniqueConnection);
            // connections
            qDebug()<<serial<<serial->portName();
            sendSetupLines(); // if any
            // test
            // sendSplitOn();
            // QTimer::singleShot(1000, this, &ARCON::sendSplitOff);
            // end test
        }
    }

}

// The state of the TCP socket has changed, so react.
void ARCON::onSocketStateChanged(QAbstractSocket::SocketState state) {
    switch(state) {
    case QAbstractSocket::UnconnectedState:qDebug()<<"Socket Disconnected";emit socketDisconnected(); break;
    case QAbstractSocket::HostLookupState:break;
    case QAbstractSocket::ConnectingState:break;
    case QAbstractSocket::ConnectedState:onSocketConnected();break;
    case QAbstractSocket::BoundState:break;
    case QAbstractSocket::ListeningState:break;
    case QAbstractSocket::ClosingState:break;
    default:break;
    }
}

// The TCP socket to the radio has been connected, so do the setup commands.
void ARCON::onSocketConnected() {
    // sent setup commands defined in the radio control file
    socket->write("\r\r"); // wake up the socket, just in case (CODAN)
    sendSetupLines(); // the initial setup lines which are only run ONCE
    qDebug()<<"Socket Connected"<<socket->localAddress()<< socket->localPort();
    emit socketConnected(socket->peerAddress().toString(), socket->peerPort());
}

// When a chunk of data comes over the radio connection, build the input
// buffer until there is no more to swallow.  Timer timeout will trigger
// onRadioDataIn();
void ARCON::onReadyRead() {
    portTimer.stop();
    // Reading the serial/socket buffer is required, even if the data is not used
    // for anything, so that port buffers do not overflow.
    if(isTcp && socket && socket->state() == QTcpSocket::ConnectedState) {
        inbytes.append(socket->readAll());
    }
    else if(serial) { // otherwise as long as the serial port is instantiated
        inbytes.append(serial->readAll());
    }
    portTimer.start();
}

// When remote control data comes over the radio connection, handle it.
void ARCON::onRadioDataIn() {
    portTimer.stop(); // will be auto-restarted by readyRead signal
    qDebug()<<"onRadioDataIn:"<<freqType<<(freqType == "CAT"?inbytes:inbytes.toHex());
    QString in = freqType == "CAT"?inbytes:inbytes.toHex().toUpper();
    QStringList cmds = in.split((freqType == "CI-V")?"FEFE":freqSuffix, Qt::SkipEmptyParts);
    inbytes.clear();
    foreach(QString cmd, cmds) {
        // qDebug()<<"query raw:"<<cmd<<"trimmed:"<<cmd.mid(0,freqPrefixA.length());

        // Check the CI-V commands for ack/nack and add prefix back for matching
        // add the FEFE back to CI-V commands so easier to search for
        // from send cmd lists
        if(freqType == "CI-V") {
            if(cmd.endsWith("FBFD")) { return; } // ACK
            else if(cmd.endsWith("FAFD")) {
                if(b_cmdDebug) {
                    if(client && client->state() == QTcpSocket::ConnectedState) {
                        client->write("NACK: " + cmd.toUtf8());
                    }
                }
                return;
            } // NACK
            cmd = cmd.prepend("FEFE");
        }
        if(b_cmdDebug) {
            if(client && client->state() == QTcpSocket::ConnectedState) {
                client->write(cmd.toUtf8() + CRLF);
            }
        }
        if(client && client->state() == QTcpSocket::ConnectedState) {
            if(!vfoAResponse.isEmpty() && cmd.startsWith(vfoAResponse)) {
                qDebug()<<"got vfoa";
                client->write("vfoa" + CRLF);
                client->flush();
            }
            else if(!vfoBResponse.isEmpty() && cmd.startsWith(vfoBResponse)) {
                qDebug()<<"got vfob";
                client->write("vfob" + CRLF);
                client->flush();
            }
            else if(!modeResponsePrefix.isEmpty() && cmd.startsWith(modeResponsePrefix)) {
                qDebug()<<"got mode"<<cmd;
                // by looking up against the modeList values
                if(freqType == "CAT") {
                    foreach (QString mode, modeList.values()) {
                        qDebug()<<"CAT modeList mode:"<<mode;
                        if(mode.startsWith(cmd)) {
                            //qDebug()<<"found mode:"<<modeList.key(mode);
                            client->write(("mode " % modeList.key(mode.toUpper()) % CRLF).toUtf8());
                            client->flush();
                            break;
                        }
                    }
                }
                else if(freqType == "CI-V") {
                    QStringList prefixes = modeResponsePrefix.split(",", Qt::SkipEmptyParts);
                    foreach(QString pre, prefixes){
                        if(cmd.startsWith(pre)) {
                            qDebug()<<"CI-V cmd match:"<<cmd;
                            foreach(QString civmode, modeList.values()) {
                                QString mode = civmode.mid(10);
                                qDebug()<<"civ mode:"<<mode<<"cmd:"<<cmd;
                                if(cmd.contains(mode)) {
                                    qDebug()<<"found mode:"<<mode;
                                    qDebug()<<"key:"<<modeList.key(civmode);
                                    break;
                                }
                            }
                        }
                    }

                }
            }
            else if(cmd.startsWith(vfoAFreqResponse)) {
                qDebug()<<"vfoa freq response:"<<cmd;
                if(freqType == "CAT") {
                    client->write(("vfoa " % cmd.mid(freqPrefixA.length()) % CRLF).toUtf8());
                    client->flush();
                }
                else if(freqType == "CI-V") {
                    // const int lenny = freqPrefixA.length();
                    client->write(("vfoa " % getCIVFreq(cmd) % CRLF).toUtf8());
                    client->flush();
                }
            }
            else if(cmd.startsWith(vfoBFreqResponse)) {
                qDebug()<<"vfoa freq response:"<<cmd;
                if(freqType == "CAT") {
                    client->write(("vfob " % cmd.mid(freqPrefixB.length()) % CRLF).toUtf8());
                    client->flush();
                }
                else if(freqType == "CI-V") {
                    // const int lenny = freqPrefixA.length();
                    client->write(("vfob " % getCIVFreq(cmd) % CRLF).toUtf8());
                    client->flush();
                }
                else if (freqType == "BCD") {
                    client->write(("vfob " % getBCDFreq(cmd) % CRLF).toUtf8());
                    client->flush();
                }
            }
        }
    }
}

// Send one command to the radio based on radio control type (freqType)
void ARCON::sendRadioCommand(const QByteArray cmd) {
    //qDebug()<<"Send Radio Command:"<<cmd + freqSuffix.toUtf8();
    // Send the derived actual radio command here
    // and force flush
    QByteArray out = cmd;
    if(cmd.trimmed().isEmpty()) { return; }
    if(freqType == "CAT") {out.append(freqSuffix.toUtf8()); }
    else if(freqType == "CI-V") { out.append(QByteArray::fromHex(freqSuffix.toUtf8())); }
    // BCD is already fully formed as "cmd"
    qDebug()<<"sendRadioCommand out:"<<out;
    if(Q_UNLIKELY(isTcp)) {
        if(socket) {
            socket->write(out);
            socket->flush();
        }
    }
    else {//qDebug()<<"serial out:"<<out;
        if(serial) {
            serial->write(out);
            serial->flush();
        }
    }
}

// for ICOM radios with their jumbled bs frequency setting method
QString ARCON::buildCIVFreq(const QString freq, const int length, int vfo)
{
    // Do the CI-V frequency gymnastics and padding out to [length] digits
    QString out;
    QString outfreq = freq;
    while(outfreq.length() < length) {
        outfreq.prepend('0');
    }
    qDebug()<<"buildCIVFreq outfreq:"<<outfreq;
    // 17 44 35 00 =  17443500 Hz --> 00 35 44 17 00
    switch(length) {
    case 10: out = outfreq.mid(8, 2) % outfreq.mid(6, 2) % outfreq.mid(4, 2) % outfreq.mid(2, 2) % outfreq.mid(0, 2);break;
    case 12: out = outfreq.mid(10, 2) % outfreq.mid(8, 2) % outfreq.mid(6, 2) % outfreq.mid(4, 2) % outfreq.mid(2, 2) % outfreq.mid(0, 2);break;
    default: out = outfreq.mid(6, 2) % outfreq.mid(4, 2) % outfreq.mid(2, 2) % outfreq.mid(0, 2);break;
    }
    // qDebug()<<"buildCIVFreq:"<<out;
    // probably don't need to choose between VFOs because ICOM is stupid
    return (vfo == 0 ? freqPrefixA : freqPrefixB) % out % freqSuffix;
}

QString ARCON::getCIVFreq(const QString freqHz) {
    qDebug()<<"getCIVFreq:"<<freqHz<<freqPrefixA<<freqSuffix<<freqLength;
    const int idx = freqPrefixA.length();
    const int endex = freqHz.indexOf(freqSuffix, idx);
    const QString outCIV = freqHz.mid(idx, endex - idx); // FEFEE0A403 0005131000 FD == 10130500 Hz
    qDebug()<<"freq digits:"<<idx<<endex<<outCIV;
    QString out;
    switch(freqLength) {
    case 8: out = outCIV.mid(6 ,2) % outCIV.mid(4, 2) % outCIV.mid(2,2) % outCIV.mid(0, 2); break;
    case 10:out = outCIV.mid(8, 2) % outCIV.mid(6 ,2) % outCIV.mid(4, 2) % outCIV.mid(2,2) % outCIV.mid(0, 2); break;
    case 12:out = outCIV.mid(10, 2) % outCIV.mid(8, 2) % outCIV.mid(6 ,2) % outCIV.mid(4, 2) % outCIV.mid(2,2) % outCIV.mid(0, 2); break;
    default:out = "???"; break;
    }
    qDebug()<<"getCIVFreq out:"<<out;
    while(out.at(0) == "0") { out.removeFirst(); }
    return out;
}

// Use this function for the old Yaesu BCD commands (BINARY)
QString ARCON::buildCATFreq(const QString freq, const int length, int vfo) {
    qDebug()<<"buildCATFreq:"<<freq<<length;
    QString outfreq = freq;
    while (outfreq.length() < length) {
        outfreq.prepend('0');
    }
    return (vfo == 0? freqPrefixA : freqPrefixB) % outfreq % freqSuffix;
}

QString ARCON::getCATFreq(const QString freqHz) {
    const int idx = freqPrefixA.length();
    const int endex = freqHz.indexOf(freqSuffix);
    return freqHz.mid(idx, endex - idx);
}

QString ARCON::buildBCDFreq(const QString freq, const int length, const QString suffix) {
    // preserved for some other esoteric BCD frequency work
    // qDebug()<<"buildBCDFreq freq:"<<freq<<"length:"<<length<<"suffix:"<<suffix;
    if(freq.length() < length) {
        return "";
    }
    QString out;
    for(int i = 0; i < length; ++i) {
        // qDebug()<<"buildBCDFreq char:"<<freq.at(i);
        out.append(freq.at(i));
    }
    return out % suffix;
}

QString ARCON::getBCDFreq(const QString freqHz) {
    // qDebug()<<"getBCDFreq:"<<freqHz<<freqLength;
    return freqHz.mid(0, freqLength);
}

// void ARCON::processRadioLine(const QString line) {
//     const QString test = line.toLower().simplified(); // removes extra spaces between tokens
//     if(!test.trimmed().isEmpty() && !test.startsWith("radio") && !test.startsWith("#")) {
//         // the only thing left is the name of the radio definition
//         radioName = test.trimmed().toUpper();
//         //qDebug()<<"Radio Name:"<<radioName;
//     }
//     else {
//         if(test.contains(" txs ")) {
//             // add to tx vector
//             QString toadd = test;
//             toadd = test.mid(test.indexOf(" txs ") + 5).trimmed();
//             if(toadd.startsWith("hex")) {
//                 toadd = toadd.mid(4);
//             }
//             else if (toadd.startsWith("ascii ")){
//                 toadd = toadd.mid(6);
//             }
//             txsLines<<toadd.trimmed();
//         }
//         else if(test.contains(" rxs ")) {
//             // add to rx vector
//             QString toadd = test;
//             toadd = test.mid(test.indexOf(" rxs ") + 5).trimmed();
//             if(toadd.startsWith("hex")) {
//                 toadd = toadd.mid(4);
//             }
//             else if(toadd.startsWith("ascii ")){ toadd = toadd.mid(6); }
//             rxsLines<<toadd.trimmed();
//         }
//         else if(test.contains(" setup ")) {
//             // add to setup vector
//             QString toadd = test;
//             toadd = test.mid(test.indexOf(" setup ") + 7).trimmed();
//             if(toadd.startsWith("hex")) {
//                 toadd = toadd.mid(4);
//             }
//             else if(toadd.startsWith("ascii ")) { toadd = toadd.mid(6); }
//             setupLines<<toadd.trimmed();
//         }
//         else if(test.contains(" ptton ")) {
//             // set the ptton string
//             ptton = test.mid(test.indexOf(" ptton ") + 7).trimmed();
//             if(ptton.startsWith("hex ")) {
//                 ptton = ptton.mid(4);
//             }
//             else {
//                 ptton = ptton.mid(6);
//             }
//         }
//         else if(test.contains(" pttoff ")) {
//             // set the pttoff string and hex vs ascii value
//             pttoff = test.mid(test.indexOf(" pttoff ") + 8).trimmed();
//             if(pttoff.startsWith("hex ")) {
//                 pttoff = pttoff.mid(4);
//                 isBinary = true;
//             }
//             else {
//                 pttoff = pttoff.mid(6);
//                 isBinary = false;
//             }
//         }
//     }
// }

