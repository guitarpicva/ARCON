Advanced Radio CONtrol

Using a more MVVM approach to radio control [by creating each radio's model using JSON](https://github.com/guitarpicva/ARCON/wiki).

See also [RSON](https://github.com/guitarpicva/RSON.git) and [RadioFiles](https://github.com/guitarpicva/RadioFiles) repos.

Starting the program from the command line currently requires the LAST parameter be the path to the RSON radio json file to load.  For the reference implementation, ensure that the RSON file is correct for mainly the serial port data.  Further work on extending the command line parameters to include serial port/ip address and baud rate/port number will be in the next iteration.

## Building ARCON
Dart language dependency for building ARCON is *min version of 3.12.2* for the Dart SDK.

ARCON is written in the Dart programming language.  If the Dart SDK is installed on the target system, a command line build is quite simple from the base arcon source code directory.

Dependency for serial port access is libserialport.  The serialport.dll file is provided for Win64 in the Win x64 reference implementation installer as well as in the repository.  This file is required for the Win64 version to function properly.

For Linux the system package is usually named libserialport-dev, which is used to BUILD the static executable using the `dart build ...` utility.  Typically, the libserialport library need not be installed on the target system (but likely would be anyway).


## Steps after cloning the repository

From the arcon source code base directory:
1. `dart pub get`

Step 1. ensures all dependencies are met for the build as contained in the pubspec.yaml file.

2. `dart build cli bin/arcon.dart`

Step 2. creates a ./build folder structure and makes the project.

3. Find the static executable

After the dart build step, Dart will print the path to the created executable named `arcon` or `arcon.exe` based on platform.

4. As an example: `./arcon RadioFiles/FT-450D.json`

Run the arcon program with a single required parameter of the chosen RSON file for the radio to be controlled.  Use the `-h` or `--help` switch to see the other command line paramaters available, such as the radio address value or the radio connection port number or serial baud rate value.  Other parameters may be added in the future.

5. For command line help: `./arcon --help`
```
Usage: dart arcon.dart <flags> [arguments]
-h, --help            Print this usage information.
    --version         Print the tool version.
    -s, --secure          The server will use TLS to cover incoming connections.
-a, --address         The radio TCP address or serial port name. i.e. `-a COM7`
-p, --port_or_baud    The radio TCP port or Serial baud rate. i.e. `-p 19200`

```