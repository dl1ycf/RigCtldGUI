# RigCtldGUI
A graphical user interface for rig control, offering also "rigctld" functionality

This program is based on the "hamlib" rig control library (also contained in my GitHub account
and at many other places). It offers to control the rig through a small graphical user interface.

What is currently implemented is

- change RF output power
- change speed of CW keyer in the rig
- change mode (CW, LSB, USB, USB-DATA)
- send pre-defined CW texts (CQ calls etc.)
- send Voice samples ("voice keyer"
- send single-tone and two-tone signal
- go to "TUNE" mode with 10 or 25 watt

Furthermore, it runs a "rigctld" daemon. That is, once this program has connected to a rig,
you can control the rig through other programs such as fldigi, wsjtx etc. by connecting
to a "Hamlib NET rigctl" virtual rig to TCP port 4532.

This way, you can, for example, run a digimode program and change the RF output power
by clicking the appropriate button on this GUI.

The graphical user interface uses the FLTK library. When compiling the program
REQUIRES that the "hamlib" source tree is present and located at "../hamlib".
This is so because some source code files of hamlib are compiled with the program.
It is also necessary that hamlib is installed, such that e.g. hamlib include files

#include <hamlib/rig.h>

are found.

There is a (preliminary) small documentation in the files

RigControllerManual.odt
RigControllerManual.pdf



