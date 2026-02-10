# RigCtldGUI

This is the original hamlib "rigctld" (Rig Control Daemon), equipped with
a graphical user interface. To this end, the hamlib repository is down-loaded
and compiled when "making" the program (this is done in the shell script
build-hamlib.sh).

This program, once connected to a rig, can accept several connections from other
programs (fldigi, wsjtx, logbook program, etc.) via TCP and connect them with the rig.
This way many programs can "talk" to the rig even if that rig only offers
a single connection through a serial or USB cable.

What is currently implemented is

- change RF output power
- change speed of CW keyer in the rig
- change mode (CW, LSB, USB, USB-DATA)
- send pre-defined CW texts (CQ calls etc.)
- send auto from .wav files ("voice keyer")
- send single-tone and two-tone signal
- go to "TUNE" mode with 10 or 25 watt

To connect to the rig via this program choose
"Hamlib NET rigctl" as the radio (in the digimode or logbook program) and for the interface, choose
one of the following possibilities

  :4532
  localhost:4532
  127.0.0.1:4532

(one of those is usually the default, that is,
TCP port 4532 on the local computer). So the digimode program(s) communicates with
the rig controller which in turn communicates with the radio. It is also possible
that a connection from another computer takes place.

This way, you can, for example, run a digimode program and change the RF output power
by clicking the appropriate button on this GUI, it is also possible to connect from
several programs (say, fldigi and a logbook program) to your rig at the same time.

The graphical user interface uses the FLTK library, so a developer version of FLTK
must be installed. Compiling hamlib requires the "autoconf" package.
For the voice keyer and single/two tone experiments one needs the PortAudio
library. The following packages should be installed

a) packages needed for compilation in general
=============================================

sudo apt-get install gcc \
sudo apt-get install g++ \
sudo apt-get install make \
sudo apt-get install autoconf \
sudo apt-get install automake \
sudo apt-get install libtool

b) packages needed for RigCtldGUI
=================================

sudo apt-get install portaudio19-dev \
sudo apt-get install libfltk1.3-dev



There is a (preliminary) small documentation in the files

RigControllerManual.odt \
RigControllerManual.pdf



