# ARCON Roadmap

Basic functionality is to accommodate use by existing software programs in a simple manner with few changes to the existing software system.

The addition of other functionality will extend the usefulness of ARCON using a more industry standard method.

## Current Activites in Progress

* Add TLS Connectivity -- largely done and tested with "localhost" certs/key which is in the repo.  Cert/Key activities need a repeatable process to help users to do this easily, whithout a lot of pain and anquish.  I believe in Linux this is rather simple with a shell script, but in Windows that's another story.

* Started the addition of "user_commands": {[]} section of the RSON files.  User Commands should allow users to create custom command names for integration into other software or for their own personal convenience.  Intent is to add a tab in the RSON UI application to add individual user commands to the RSON file, and a way to print them out for viewing the list.  Open question is whether to make it a more complex command set, like the existing query commands, or simply a way to send command lists.  It is rare for properly formatted commands to fail, so at least for the initial extension activity, there will be only a way to store a command list by name.

## Upcoming Features in Plan

* Decide on How to do Control of Commercial Radios

This may require a fork due to various idiosyncracies of commercial radio firmware.  Every manufacturer is potentially completely different.  This would be a more "Hamlib" sort of solution but since the list is somewhat short, not out of the realm of possiblilty.  CURRENTLY: CODAN radios have basic functionality, as well as the ICOM IC-F8101E using the current ARCON system.

* Control of Ancillary Devices

Such as data modems or various linking systems (2G/3G/4G ALE, DStar data, Pactor, etc.)

* MQTT Client Capabilities

The addition of industry standard message passing using MQTT will facilitate the ability to have radio systems work in concert with one another as a "unit".  This work is largely done already in other projects, so it may be realized rather quickly.

* WebSocket server side at the radio

A very simple websocket http service could be added to pass the commands and indications so that web clients may easily participate.  This service will be off by default and may be enabled at startup only.
