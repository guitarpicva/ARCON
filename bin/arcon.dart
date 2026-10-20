// temp
import 'dart:io';

import 'package:arcon/arcon.dart';
import 'package:args/args.dart';

const String version = '0.1.0';

ArgParser buildParser() {
  return ArgParser()
    ..addFlag(
      'help',
      abbr: 'h',
      negatable: false,
      help: 'Print this usage information.',
    )
    ..addFlag('version', negatable: false, abbr: 'v', help: 'Print the tool version.')
    ..addFlag('secure', negatable: true, abbr: 's', help: 'Start the server in TLS mode.')
    ..addFlag('tcpServer', negatable: true, abbr: 'S', help: 'Start the server in TCP mode. true or false')
    ..addFlag('websocket', abbr: 'w', negatable: true, help: 'Start the websocket server.')
    ..addOption('address', abbr: 'a', help: 'The radio TCP address or serial port name. e.g. -a COM7')
    ..addOption('port_or_baud', abbr: 'p', help: 'The radio TCP port or Serial baud rate. e.g. -p 19200');
}

void printUsage(ArgParser argParser) {
  print('Usage: dart arcon.dart <flags> [arguments]');
  print(argParser.usage);
}

void main(List<String> arguments) {  
  String address = '127.0.0.1';
  String port = '19791';
  bool secure = false;
  bool websocket = false;
  bool tcpServer = false;
  final ArgParser argParser = buildParser();
  try {
    final ArgResults results = argParser.parse(arguments);

    // Process the parsed arguments.
    if (results.flag('help')) {
      printUsage(argParser);
      return;
    }
    if (results.flag('version')) {
      print('arcon version: $version');
      return;
    }
    if(results.flag('secure')) {
      secure = true;
    }
    if(results.flag('websocket')) {
      websocket = true;
    }
    if(results.flag('tcpServer')) {
      tcpServer = true;
    }
    address = results.option('address')??'';
    port = results.option('port_or_baud')??'';    
    print('option address: $address');
    print('option port_or_baud: $port');
    print('option secure: $secure');
    print('option startserver: $tcpServer');
    print('option websocket: $websocket');
    // Act on the arguments provided.
    print('Positional arguments: ${results.rest}');
  } on FormatException catch (e) {
    // Print usage information if an invalid argument was provided.
    print(e.message);
    print(
      'Usage: arcon [flags] [RSON radio file name]\n\nRadio filename must be the last parameter.',
    );
    printUsage(argParser);
  }
  // start the process with the radio filename gathered from
  // the command line args as the very last argument.
  // ARCON(arguments.last);
  // TEST
  print('CWD: ${Directory.current}');
  print('Starting ARCON: $arguments');
  ARCON(arguments.last, address:address, port:port, secure:secure, startServer: tcpServer, websocket: websocket);
  // END TEST
}
