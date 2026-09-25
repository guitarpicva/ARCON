import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:convert/convert.dart';
import 'package:libserialport/libserialport.dart';

String freqPrefixA = '';
String freqPrefixB = '';
String freqSuffix = '';
String freqType = '';
String radioAddress = '';
String radioFile = '';
String radioName = '???';
String serialFlowControl = 'No Flow Control';
String serialParams = '8N1';
String pttOn = '';
String pttOff = '';
String pttData = '';
String splitOn = '';
String splitOff = '';
String splitToggle = '';
String toVFOA = '';
String toVFOB = '';
String autoTune = '';
String modeQuery =
    ''; // which mode is in use -- compare reply to modeList values
String vfoQuery = ''; // which vfo is in use
String vfoAFreq = ''; // query VFO A frequency
String vfoBFreq = ''; // query VFO B frequency
String vfoAResponse = ''; // expected response for VFO query where curr vfo is A
String vfoBResponse =
    ''; // expected response for VFO B query where curr vfo is B
String vfoAFreqResponse = ''; // expected respoinse for VFO A frequency query
String vfoBFreqResponse = ''; // expected respoinse for VFO B frequency query
String modeResponsePrefix =
    ''; // expected prefix of mode query reply // MDO for test only
Map<String, String> modeList =
    <String, String>{}; // list of modes defined for this radio
final String crlf = '\r\n';
int freqLength = 10; // most common value
int radioTcpPortNumber = 23;
int serverListenPortNumber = 19791;
int serialBaudRate = 19200;
int txTail = 20;
bool isTcp = false;
bool bStartServer = true; // hard coded on for now
bool bCmdDebug = false; // user can turn it on via the TCP client
late SerialPort serial; // Serial connection to radio
late Socket socket; // TCP connection to radio
late ServerSocket server;
late Socket client;
List<int> clientBytes = <int>[];
List<int> inbytes = <int>[];
List<String> cmdLines = <String>[];
List<String> setupLines = <String>[];
final String clientHelpText =
    'ARCON Client Help\n\nCommand List:\nptton\npttoff\npttdata\nvfoafreq\nvfobfreq\nspliton\nsplitoff\ntovfoa\ntovfob\nautotune\nQuery Commands:\n?mode\n?vfo\n?vfoa\n?vfob\n';

class ARCON {
  /// The constructor which requires
  ARCON(
    String radioFilename, {
    bool startServer = true,
    int serverPortNumber = 19791,
  }) {
    print('RadioFile: $radioFilename');
    radioFile = radioFilename;
    bStartServer = startServer;
    serverListenPortNumber = serverPortNumber;

    // load the model for the radio to connect to
    loadRadioFile();
    // timers for ports were here in C++
    // start the connection to the radio
    startControlConnection();
    // if we wish to start the control TCP
    // server do so now.
    if (bStartServer) {
      startARCONServer();
    }
  }

  Future<ServerSocket> startARCONServer() async {
    // only one active connection to the radio (for now)
    var ss = await ServerSocket.bind(
      InternetAddress.anyIPv4,
      19791,
      shared: false,
    );
    server = ss;
    server.listen((client) {
      newConnection(client);
      print(
        'Server Socket started on address:port ${client.address}${client.port}',
      );
    });

    return server;
  }

  void newConnection(Socket clientSocket) {
    client = clientSocket;
    client.setOption(SocketOption.tcpNoDelay, true);
    print('TCP client connected...');
    client.listen(
      (Uint8List data) async {
        processClientData(data);
      },
      cancelOnError: false,
      onError: (error) {
        print('TCP client error: $error');
      },
      onDone: () {
        print('TCP client finished...');
        client.close();
      },
    );
  }

  /// Process data arriving on the TCP control socket connection.
  void processClientData(Uint8List data) {
    // this is an ASCII command port only, so split the commands
    // on line end and pass to handler function [handleClientcommands].
    //print('got client data: $data');
    var inbytes = String.fromCharCodes(data.toList());
    List<String> cmds = inbytes.split('\n');
    //print(cmds);
    handleClientCommands(cmds);
  }

  void handleClientCommands(final List<String> cmds) {
    for (String cmd in cmds) {
      print('handleClientCommands: $cmd');
      if (cmd.startsWith("?")) {
        // send to the query handler
        queryCmd(cmd.substring(1));
      }
      if (cmd.startsWith("ptton")) {
        sendPttOn();
      } else if (cmd.startsWith("pttdata")) {
        sendPttData();
      } else if (cmd.startsWith("pttoff")) {
        sendPttOff();
      } else if (cmd.startsWith("vfoafreq")) {
        List<String> parts = cmd.split(' ');
        print('Parts:$parts');
        if (parts.length < 2) {
          return;
        }
        final String freq = parts[1];
        if (freq.isEmpty) {
          return;
        }
        sendSetVFOFreq(freq);
      } else if (cmd.startsWith("vfobfreq")) {
        List<String> parts = cmd.split(' ');
        print('parts: $parts');
        if (parts.length < 2) {
          return;
        }
        final String freq = parts[1];
        if (freq.isEmpty) {
          return;
        }
        sendSetVFOFreq(freq, vfo: 1);
      } else if (cmd.startsWith("mode")) {
        List<String> parts = cmd.split(' ');
        if (parts.length < 2) {
          return;
        }
        final String mode = parts[1];
        if (mode.isNotEmpty) {
          print('sendSetMode: $mode');
          sendSetMode(mode);
        } else {
          print('mode is empty...:(');
        }
      } else if (cmd.startsWith("tovfoa")) {
        sendToVFOA();
      } else if (cmd.startsWith("tovfob")) {
        print('trapped client cmd: tovfob');
        sendToVFOB();
      } else if (cmd.startsWith("autotune")) {
        sendAutoTune();
      } else if (cmd.startsWith("setuplines")) {
        sendSetupLines();
      } else if (cmd.startsWith("spliton")) {
        sendSplitOn();
      } else if (cmd.startsWith("splitoff")) {
        sendSplitOff();
      } else if (cmd.startsWith("splittoggle")) {
        sendSplitToggle();
      }
      // else if(cmd.startsWith("txslines") {sendTxsLines();}
      // else if(cmd.startsWith("rxslines") {sendRxsLines();}
      else if (cmd.startsWith("help")) {
        client.add(clientHelpText.codeUnits);
      } else if (cmd.startsWith("restart")) {
        startOver(radioFile);
        var msg = 'restarting ${radioFile.split("/").last}$crlf';
        client.add(msg.codeUnits);
        print('restart with current radio file ${radioFile.split("/").last}');
        return; // just in case things are added after this
      } else if (cmd.startsWith("debug")) {
        List<String> parts = cmd.toLowerCase().split(' ');
        if (parts.length > 1) {
          bCmdDebug = parts[1] == "on";
          var msg = 'cmd debug: ${parts[1]}$crlf';
          client.add(msg.codeUnits);
        }
      }
    }
  }

  void queryCmd(final String cmd) {
    String out;
    // decide which query cmd to use based on the API command passed
    if (cmd.trim().isEmpty) {
      return;
    }
    out = cmd.toUpperCase(); // TODO: TEMPORARY TEST for raw commands
    if (cmd.startsWith("vfoa")) {
      out = vfoAFreq;
    } else if (cmd.startsWith("vfob")) {
      out = vfoBFreq;
    } else if (cmd.startsWith("vfo")) {
      out = vfoQuery;
    } else if (cmd.startsWith("mode")) {
      out = modeQuery;
      print('mode query: $out');
    }
    for (String line in out.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    print('queryCommands: $cmdLines ${cmdLines.length}');
    sendCommands();
  }

  void sendCommands() {
    // send first command from cmdLines and remove first
    Timer.periodic(Duration(milliseconds: 50), (t) {
      // print('cmd timer pop -- $cmdLines');
      if (cmdLines.isEmpty) {
        t.cancel();
        return;
      }
      if (freqType == "CAT") {
        var cmd = Uint8List.fromList(cmdLines[0].codeUnits);
        // print('send cat command -- $cmd');
        sendRadioCommand(cmd);
      } else {
        var fromhex = hex.decode(cmdLines[0]);
        var cmd = Uint8List.fromList(fromhex);
        sendRadioCommand(cmd);
      }
      cmdLines.removeAt(0);
      if (cmdLines.isEmpty) {
        t.cancel();
      }
    });
  }

  void startOver(final String radioFilename) {}

  void loadRadioFile() {
    // using radioFile path, set up all instance variables
    var f = File(radioFile);
    if (f.existsSync()) {
      var jsonin = f.readAsBytesSync().toList();
      String jsonString = String.fromCharCodes(jsonin);
      final Map<String, Object?> jsonMap =
          jsonDecode(jsonString) as Map<String, dynamic>;
      // print('json Decoded: $jsonMap');
      radioName = jsonMap['radioName'].toString();
      isTcp = jsonMap['connectionMethod'].toString() == "SERIAL" ? false : true;
      radioAddress = jsonMap['radioAddress'].toString();
      serialBaudRate = int.parse(jsonMap['serialBaudRate'].toString());
      serialParams = jsonMap['serialParams'].toString();
      serialFlowControl = jsonMap['serialFlowControl'].toString();
      radioTcpPortNumber = int.parse(jsonMap['radioTcpPortNumber'].toString());
      List<String> sltmp = jsonMap['initialSetup'].toString().split(",");
      for (String s in sltmp) {
        if (s.trim().isNotEmpty) {
          setupLines.add(s);
        }
      }

      pttOn = jsonMap['pttOn'].toString();
      pttOff = jsonMap['pttOff'].toString();
      pttData = jsonMap['pttData'].toString();
      txTail = int.parse(
        jsonMap['txTail'].toString(),
      ); // def. 20 ms for bad/missing value
      splitOn = jsonMap['splitOn'].toString();
      splitOff = jsonMap['splitOff'].toString();
      toVFOA = jsonMap['toVFOA'].toString();
      toVFOB = jsonMap['toVFOB'].toString();
      autoTune = jsonMap['autoTune'].toString();
      // modes list Map<String, String>
      var modetmp = jsonEncode(jsonMap['modeList']);
      // print('modetmp: $modetmp');
      if (modetmp != null) {
        var jsonModes = jsonDecode(modetmp);
        // print('jsonModes: $jsonModes -- ${jsonModes.keys}');
        for (String key in jsonModes.keys) {
          modeList[key] = jsonModes[key];
        }
        // print('modeList: $modeList');
      }

      var freqtmp = jsonEncode(jsonMap['frequencyControl']);
      if (freqtmp != null) {
        var freqValues = jsonDecode(freqtmp);
        freqLength = int.parse(freqValues['numDigits'].toString());
        freqType = freqValues['order'].toString();
        freqPrefixA = freqValues['prefixA'].toString();
        freqPrefixB = freqValues['prefixB'].toString();
        freqSuffix = freqValues['suffix'].toString();
      }
      var querytmp = jsonEncode(jsonMap['queryCommands']);
      if (querytmp != null) {
        var queryValues = jsonDecode(querytmp);
        modeQuery = queryValues['mode'].toString();
        modeResponsePrefix = queryValues['modeResponsePrefix'].toString();
        vfoQuery = queryValues['vfo'].toString();
        vfoAResponse = queryValues['vfoAResponse'].toString();
        vfoBResponse = queryValues['vfoBResponse'].toString();
        vfoAFreqResponse = queryValues['vfoAFreqResponse'].toString();
        vfoBFreqResponse = queryValues['vfoBFreqResponse'].toString();
        vfoAFreq = queryValues['vfoAFreq'].toString();
        vfoBFreq = queryValues['vfoBFreq'].toString();
      }
      //     // may wish to add ad-hoc mode list here later, from a new
      //     // section of the JSON maybe called userModeList {}
      // print('Radio Name:$radioName');
      // print('TCP Radio? $isTcp');
      // print('setup: $setupLines');
      // print('pttOn: $pttOn');
      // print('pttOff: $pttOff');
      // print('txTail: $txTail');
      // print('splitOn: $splitOn');
      // print('splitOff: $splitOff');
      // print('radioTcpPortNumber: $radioTcpPortNumber');
      // print('freqType: $freqType');
      // print('freqLength: $freqLength');
      // print('freqPrefix: $freqPrefixA $freqPrefixB');
      // print('freqSuffix: $freqSuffix');
      // print('modeList: $modeList');
      // print('modeQuery: $modeQuery');
      // print('modeResponsePrefix: $modeResponsePrefix');
      // print('vfoQuery: $vfoQuery');
      // print('vfoAResponse: $vfoAResponse');
      // print('vfoBResponse: $vfoBResponse');
      // print('vfoAFreq: $vfoAFreq');
      // print('vfoBFreq: $vfoBFreq');
      // print('toVFOA: $toVFOA');
      // print('toVFOb: $toVFOB');
    }
  }

  void sendSetupLines() {
    //print('Send setup lines $setupLines');
    for (String line in setupLines) {
      print('$line$freqSuffix');
      cmdLines.add('$line$freqSuffix');
    }
    //  = setupLines;
    sendCommands(); // manages a timer to space out the commands
    // until done, then cancels the timer.
  }

  void sendPttOn() {
    for (String line in pttOn.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    // cmdLines = pttOn.split(',');
    print('cmdLines for pttOn: $pttOn $cmdLines');
    sendCommands();
  }

  void sendPttOff() {
    for (String line in pttOff.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendPttData() {
    for (String line in pttData.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendSplitOn() {
    for (String line in splitOn.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendSplitOff() {
    for (String line in splitOff.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendSplitToggle() {
    for (String line in splitToggle.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendSetMode(final String modeName) {
    // gather the mode command by name from the [modeList]
    var modecmd = modeList['modeName'];
    if (modecmd != null && modecmd.isNotEmpty) {
      for (String line in modecmd.split(',')) {
        cmdLines.add('$line$freqSuffix');
      }
      sendCommands();
    }
  }

  void sendSetVFOFreq(String freqHz, {int vfo = 0}) {
    String freqcmd = '';
    String freqDigits = '';
    if (freqType == "CAT") {
      freqcmd = vfo == 0 ? freqPrefixA : freqPrefixB;
      freqDigits = buildCATFreq(freqHz, length: freqLength, vfo: vfo);
      freqcmd = '$freqcmd$freqDigits$freqSuffix';
    } else if (freqType == "CI-V") {
      freqcmd = vfo == 0 ? freqPrefixA : freqPrefixB;
      freqDigits = buildCIVFreq(freqHz, freqLength, vfo: vfo);
      freqcmd = '$freqcmd$freqDigits$freqSuffix';
    } else if (freqType == "BCD") {
      freqcmd = vfo == 0 ? freqPrefixA : freqPrefixB;
      freqDigits = buildBCDFreq(freqHz, length: freqLength, suffix: '01');
      freqcmd = '$freqcmd$freqDigits$freqSuffix';
    }

    if (freqcmd.isNotEmpty) {
      // covers a rogue freqType value
      for (String line in freqcmd.split(',')) {
        cmdLines.add('$line$freqSuffix');
      }
    }
    sendCommands();
  }

  void sendToVFOA() {
    for (String line in toVFOA.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendToVFOB() {
    for (String line in toVFOB.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  void sendAutoTune() {
    for (String line in autoTune.split(',')) {
      cmdLines.add('$line$freqSuffix');
    }
    sendCommands();
  }

  Future<void> startControlConnection() async {
    if (isTcp) {
      print('get tcp radio...');
      await getTcpRadio(radioAddress, radioTcpPortNumber);
    } else {
      print('get serial radio...');
      await getSerialRadio(radioAddress, serialBaudRate);
    }
  }

  /// Connect to a serial port connected radio and map the data stream.
  Future<void> getSerialRadio(String address, int speed) async {
    try {
      bool open = false;
      var spc = SerialPortConfig();
      // seems to work fine at this speed, but slower also works
      spc.baudRate = speed;
      spc.bits = 8;
      spc.parity = 0;
      spc.stopBits = 1;
      spc.setFlowControl(SerialPortFlowControl.none);
      if (Platform.isLinux || Platform.isMacOS) {
        // print('Linux Port: $address');
        if (address.startsWith("/dev/")) {
          address = address.substring(5);
        }
        serial = SerialPort('/dev/$address'); // i.e. ttyACM0
        open = serial.openReadWrite();
        serial.config = spc;
        // spc.dtr = 1; // Windows is weird
        print('Port: $address speed: $speed');
      } else {
        // essentially Windows is the only other viable candidate ATM
        print('Port: $address speed: $speed');
        serial = SerialPort(address); // i.e. COM23
        open = serial.openReadWrite();
        spc.dtr = 1; // Windows is weird
        serial.config = spc;
      }
      if (open) {
        print("$address: OPEN!");
        sendSetupLines();
        final reader = SerialPortReader(serial);
        reader.stream.listen(
          (data) async {
            onRadioDataIn(data);
          },
          onError: (error) {
            print('Serial Port Error: ${error.toString()}');
            // Close the reader and serial port and re-open later to recover
            reader.close();
            serial.close();
            Timer(const Duration(seconds: 2), () {
              getSerialRadio(address, speed);
            });
          },
          onDone: () {
            print('Serial Port Done');
            reader.close();
            serial.close();
          },
          cancelOnError: false,
        );
      } else {
        print("$address: NOT OPEN!");
        serial.dispose();
      }
      spc.dispose();
    } catch (se) {
      // connection to radio failed, so
      // tell the UI to open the configuration Drawer
      print('$address - SerialException: ${se.toString()}');
      print("$address: NOT OPENED!");
    }
    return;
  }

  ///
  Future<void> getTcpRadio(String address, int port) async {}

  /// handle onSocketStateChanged in builder for TCP client socket
  void onSocketConnected() {}

  // When remote control data is complete over the radio connection, handle it.
  void onRadioDataIn(Uint8List data) {
    // print('onRadioDataIn: ${String.fromCharCodes(data)}');
    String indata = freqType == "CAT"
        ? String.fromCharCodes(data)
        : hex.encode(data).toUpperCase();
    List<String> cmds = indata.split((freqType == "CI-V") ? "FD" : freqSuffix);
    inbytes.clear();
    for (String cmd in cmds) {
      print('onRadioDataIn: cmd: $cmd');
      // Check the CI-V commands for ack/nack and add prefix back for matching
      // add the FEFE back to CI-V commands so easier to search for
      // from send cmd lists
      if (freqType == "CI-V") {
        if (cmd.endsWith("FB")) {
          return;
        } // ACK
        else if (cmd.endsWith("FA")) {
          if (bCmdDebug) {
            if (client != null) {
              client.add('NACK: $cmd$crlf'.codeUnits);
            }
          }
          return;
        } // NACK
      }
      if (bCmdDebug) {
        if (client != null) {
          client.add('$cmd$crlf'.codeUnits);
        }
      }
      if (client != null) {
        if (vfoAResponse.isNotEmpty && cmd.startsWith(vfoAResponse)) {
          client.add('vfoa$crlf'.codeUnits);
        } 
        else if (vfoBResponse.isNotEmpty && cmd.startsWith(vfoBResponse)) {
          print('dataIn: got vfob');
          client.add('vfob$crlf'.codeUnits);
        } 
        else if (!modeResponsePrefix.isEmpty &&
            cmd.startsWith(modeResponsePrefix)) {
          // by looking up against the modeList values
          print('dataIn: got mode $cmd');
          if (freqType == "CAT") {
            for (String mode in modeList.keys) {
              print('dataIn: CAT modeList mode: $mode');
              if (modeList[mode] == cmd) {
                print('found CAT mode: $mode');

                client.add('mode $mode$crlf'.codeUnits);
                break;
              }
            }
          } 
          else if (freqType == "CI-V") {
            List<String> prefixes = modeResponsePrefix.split(',');
            for (String pre in prefixes) {
              if (cmd.startsWith(pre)) {
                print('dataIn: CI-V cmd match: $cmd');
                for (String civmode in modeList.values) {
                  String mode = civmode.substring(10);
                  print('dataIn: civ mode: $mode cmd: $cmd');
                  if (cmd.contains(mode)) {
                    // qDebug()<<"found mode:"<<mode;
                    // qDebug()<<"key:"<<modeList.key(civmode);
                    break;
                  }
                }
              }
            }
          }
        } 
        else if (cmd.startsWith(vfoAFreqResponse)) {
          print('dataIn: vfoa freq response: $cmd');
          if (freqType == "CAT") {
            client.add(
              'vfoa ${cmd.substring(freqPrefixA.length)}$crlf'.codeUnits,
            );
          } else if (freqType == "CI-V") {
            // const int lenny = freqPrefixA.length();
            client.add('vfoa ${getCIVFreq(cmd)}$crlf'.codeUnits);
          } else if (freqType == "BCD") {
            client.add('vfob ${getBCDFreq(cmd)}$crlf'.codeUnits);
          }
        } 
        else if (cmd.startsWith(vfoBFreqResponse)) {
          print('dataIn: vfoa freq response: $cmd');
          if (freqType == "CAT") {
            client.add(
              'vfob ${cmd.substring(freqPrefixB.length)}$crlf'.codeUnits,
            );
          } else if (freqType == "CI-V") {
            // const int lenny = freqPrefixA.length();
            client.add('vfob ${getCIVFreq(cmd)}$crlf)'.codeUnits);
          } else if (freqType == "BCD") {
            client.add('vfob ${getBCDFreq(cmd)}$crlf'.codeUnits);
          }
        }
      }
    }
  }

  void sendRadioCommand(Uint8List cmd) {
    if (cmd.isEmpty) {
      return;
    }
    if (isTcp && socket != null) {
      print('sendRadioCommand: $cmd');
      socket.add(cmd.toList());
      socket.flush();
    } 
    else {
      // serial
      if (serial != null) {
        print(
          'send serial command ${hex.encode(cmd)} -- ${String.fromCharCodes(cmd)}',
        );
        serial.write(cmd);
        serial.drain();
      }
    }
  }

  // Frequency command builder utiility functions for CI-V, CAT and old Yaesu BCD
  String getCIVFreq(final String freqHz) {
    final int idx = freqPrefixA.length;
    final int endex = freqHz.indexOf(freqSuffix, idx);
    final String outCIV = freqHz.substring(
      idx,
      endex - idx,
    ); // FEFEE0A403 0005131000 FD == 10130500 Hz
    String out;
    switch (freqLength) {
      case 8:
        out =
            outCIV.substring(6, 2) +
            outCIV.substring(4, 2) +
            outCIV.substring(2, 2) +
            outCIV.substring(0, 2);
        break;
      case 10:
        out =
            outCIV.substring(8, 2) +
            outCIV.substring(6, 2) +
            outCIV.substring(4, 2) +
            outCIV.substring(2, 2) +
            outCIV.substring(0, 2);
        break;
      case 12:
        out =
            outCIV.substring(10, 2) +
            outCIV.substring(8, 2) +
            outCIV.substring(6, 2) +
            outCIV.substring(4, 2) +
            outCIV.substring(2, 2) +
            outCIV.substring(0, 2);
        break;
      default:
        out = "???";
        break;
    }
    while (out[0] == '0') {
      out = out.substring(1);
    }
    return out;
  }

  String buildCIVFreq(final String freq, final int length, {int vfo = 0}) {
    String out;
    String outfreq = freq;
    while (outfreq.length < length) {
      outfreq = '0$outfreq';
    }
    print('buildCIVFreq outfreq:$outfreq');
    // 17 44 35 00 =  17443500 Hz --> 00 35 44 17 00
    switch (length) {
      case 10:
        out =
            outfreq.substring(8, 10) +
            outfreq.substring(6, 8) +
            outfreq.substring(4, 6) +
            outfreq.substring(2, 4) +
            outfreq.substring(0, 2);
        break;
      case 12:
        out =
            outfreq.substring(10, 12) +
            outfreq.substring(8, 10) +
            outfreq.substring(6, 8) +
            outfreq.substring(4, 6) +
            outfreq.substring(2, 4) +
            outfreq.substring(0, 2);
        break;
      default:
        out =
            outfreq.substring(6, 8) +
            outfreq.substring(4, 6) +
            outfreq.substring(2, 4) +
            outfreq.substring(0, 2);
        break;
    }
    // print(<<"buildCIVFreq:"<<out;
    // probably don't need to choose between VFOs because ICOM is stupid
    if (vfo == 0) {
      out = '$freqPrefixA$out$freqSuffix';
    } else {
      out = '$freqPrefixB$out$freqSuffix';
    }
    return out;
  }

  String getCATFreq(final String freqHz) {
    final int idx = freqPrefixA.length;
    final int endex = freqHz.indexOf(freqSuffix);
    return freqHz.substring(idx, endex + 1);
  }

  String buildCATFreq(final String freq, {final int length = 8, int vfo = 0}) {
    String outfreq = freq;
    // ensure '0' front padding to [length] digits.
    while (outfreq.length < length) {
      outfreq = '0$outfreq';
    }
    // build the final command output based on vfo choice
    if (vfo == 0) {
      outfreq = '$freqPrefixA$outfreq$freqSuffix';
    } else {
      outfreq = '$freqPrefixB$outfreq$freqSuffix';
    }
    return outfreq;
  }

  String getBCDFreq(final String freqHz) {
    return freqHz.substring(0, freqLength);
  }

  String buildBCDFreq(
    final String freq, {
    final int length = 8,
    final String suffix = "01",
  }) {
    String out = '';
    for (int i = 0; i < length; ++i) {
      // print(<<"buildBCDFreq char:"<<freq.at(i);
      out += freq[i];
    }
    // ensure proper length
    while (out.length < length) {
      out = '0$out;';
    }
    return out + suffix;
  }

  /// Serial port name or IP address if TCP
  void setRadioAddress(final String portname) {
    radioAddress = portname;
  }

  /// Filename of the RSON (JSON) file to configure the radio.
  void setRadioFile(final String radiofile) {
    radioFile = radiofile;
  }

  /// TCP port number of the radio connection if required.
  void setTcpPort(final int port) {
    radioTcpPortNumber = port;
  }

  /// Whether or not the radio connection is a TCP socket.
  void setIsTcp(bool istcp) {
    isTcp = istcp;
  }

  /// Baud rate of the serial connection to the radio if required.
  void setBaudRate(final int baudrate) {
    serialBaudRate = baudrate;
  }
}
