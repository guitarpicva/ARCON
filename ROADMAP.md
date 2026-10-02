# ARCON Roadmap

Basic functionality is to accommodate use by existing software programs in a simple manner with few changes to the existing software system.

The addition of other functionality will extend the usefulness of ARCON using a more industry standard method.

## Upcoming Features in Plan


* Add TLS Connectivity

* Control of commercial radios

This may require a fork due to various idiosyncracies of commercial radio firmware.  Every manufacturer is potentially completely different.  This would be a more "Hamlib" sort of solution but since the list is somewhat short, not out of the realm of possiblilty.  CURRENTLY: CODAN radios have basic functionality, as well as the ICOM IC-F8101E using the current ARCON system.

* Control of Ancillary Devices

Such as data modems or various linking systems (2G/3G/4G ALE, DStar data, Pactor, etc.)

* MQTT Client Capabilities

The addition of industry standard message passing using MQTT will facilitate the ability to have radio systems work in concert with one another as a "unit".  This work is largely done already in other projects, so it may be realized rather quickly.
