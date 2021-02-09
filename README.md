# RigCtldGUI
A graphical user interface for rig control, offering also "rigctld" functionality

This program is based on the "hamlib" rig control library. Since parts of the code here,
for example the file rigctl_parse.c and some others, are copied from Hamlib, this program
will only compile and link well with a specific version of Hamlib present on your system.

So this program requires the release version Hamlib4.0 to be present
====================================================================

The program is essentially a "rigctld" daemon with a small graphical user interface.

What is currently implemented is

- change RF output power
- change speed of CW keyer in the rig
- change mode (CW, LSB, USB, USB-DATA)
- send pre-defined CW texts (CQ calls etc.)
- send Voice samples ("voice keyer"
- send single-tone and two-tone signal
- go to "TUNE" mode with 10 or 25 watt

Since this is functionally more or less equivalent to a "rigctld" daemon,
you can control your by other programs such as fldigi, wsjtx etc. by connecting
to a "Hamlib NET rigctl" virtual rig to TCP port 4532. Thus, wsjtx connects
to RigCtldGUI which connects to your rig.

This way, you can, for example, run a digimode program and change the RF output power
by clicking the appropriate button on this GUI, it is also possible to connect from
several programs (say, wsjtx and fldigi) to your rig at the same time.

The graphical user interface uses the FLTK library, so a "devel" version of FLTK
(version 1.3) must be installed, as well as a "devel" version of Hamlib (release
version 4.0) such that e.g. hamlib include files

#include <hamlib/rig.h>

are found.

There is a (preliminary) small documentation in the files

RigControllerManual.odt
RigControllerManual.pdf



