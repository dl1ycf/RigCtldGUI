# RigCtldGUI
A graphical user interface for rig control, offering also "rigctld" functionality

This program is based on the "hamlib" rig control library. Since parts of the code here,
for example the file rigctl_parse.c and some others, are copied from Hamlib, this program
will only compile and link well with a specific version of Hamlib present on your system.

Currently, this is meant to work with Hamlib 4.5.4
---------------------------------------------------

The "step" to Hamlib version 4.5.4 was necessary since my logbook program on my
Apple Macintosh (MacLogger DX) supports connecting to a rigctld daemon, but
this has been tested by the MLDX developer for Hamlib 4.5.4.

You can easily get a source code tree from the internet using the commands

````
  cd <where you want to create the hamlib tree>
  git clone https://github.com/hamlib/hamlib
  cd hamlib
  git checkout 4.5.4
````

The program is essentially a "rigctld" daemon with a small graphical user interface.
This allows to have several clients (e.g. a logbook program and a digimode program)
to connect to the rig which only offers a single connection through a serial or USB cable.

What is currently implemented is

- change RF output power
- change speed of CW keyer in the rig
- change mode (CW, LSB, USB, USB-DATA)
- send pre-defined CW texts (CQ calls etc.)
- send Voice samples ("voice keyer"
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

The graphical user interface uses the FLTK library, so a "devel" version of FLTK
(version 1.3) must be installed. For Hamlib, it is usually preferred to get
the source code from github and compile/install it, since the hamlib versions in
the standard repositories lag behind.

There is a (preliminary) small documentation in the files

RigControllerManual.odt
RigControllerManual.pdf



