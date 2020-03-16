########################################################################################################
#
# This Makefile assumes that a working hamlib (version 4) resides
# in ../hamlib.
#
# The following C files are taken from there:
#
# tests/dumpcaps.c
# tests/sprintflst.c
# tests/rigctl_parse.c
#
# and the following include files are used
#
# include/hamlib/rig.h                        #include <hamlib/rig.h>         used everywhere
# include/hamlib/riglist.h                    #include <hamlib/riglist.h>     via rig.h
# include/hamlib/rig_dll.h                    #include <hamlib/rig_dll.h>     via rig.h
# include/hamlib/amplifier.h                  #include <hamlib/amplifier.h>   via sprintflst.h
# include/hamlib/amplist.h                    #include <hamlib/amplist.h>     via amplifier.h
# src/misc.h                                  #include "misc.h"               via rigctl_parse.c
# src/iofunc.h                                #include "iofunc.h"             via rigctl_parse.c
# src/serial.h                                #include "serial.h"             via rigctl_parse.c
# tests/sprintflist.h                         #include "sprintflst.h"         via rigctl_parse.c
# tests/rigctl_parse.h                        #include "rigctl_parse.h"       via rigctl_parse.c
# tests/uthash.h                              #include "uthash.h"	      vial rigctl_parse.c
#
# The program is then linked against the hamlib library files in ../hamlib/src/.libs
#
# NOTE: on many systems shared libraries are used, therefore you also have to install hamlib (V. 4)
#
########################################################################################################

#
OBJS=		Rig.o Hamlib.o Winkey.o rigctl_parse.o dumpcaps.o sprintflst.o fmemopen.o
LIBS=		-lfltk -lportaudio -L ../hamlib/src/.libs -lhamlib -lreadline -lpthread
CXX=		g++
CC=		gcc
HAMINCLUDE=	-I../hamlib/include -I../hamlib/tests -I../hamlib/src 
CFLAGS=		-O $(HAMINCLUDE)
CXXFLAGS=	-O $(HAMINCLUDE)

#
# NOTE: "make Rig" makes the program
#       (MacOS, Linux)
#
Rig:	$(OBJS)
	$(CXX) -o Rig $(OBJS) $(LIBS)

rigctl_parse.o:
	$(CC) $(CFLAGS) -c $(HAMINCLUDE) -o $@ ../hamlib/tests/rigctl_parse.c

dumpcaps.o:
	$(CC) $(CFLAGS) -c $(HAMINCLUDE) -o $@ ../hamlib/tests/dumpcaps.c

sprintflst.o:
	$(CC) $(CFLAGS) -c -I../hamlib/src -I../hamlib/tests -o $@ ../hamlib/tests/sprintflst.c

clean:
	rm -rf $(OBJS) Rig Rig.app

####################################################################
#
# NOTE: "make app" is for Macintosh OSX only
#       Files only needed to build a MacOS "app":
#       PkgInfo, Info.plist, Rig.icns
#
####################################################################
app:  Rig
	@echo -n Creating AppBundle ....
	@rm -rf Rig.app
	@mkdir -p Rig.app/Contents/MacOS
	@mkdir -p Rig.app/Contents/Resources
	@mkdir -p Rig.app/Contents/Frameworks
	@cp Rig Rig.app/Contents/MacOS/Rig
	@echo -n copying libraries ...
	@cd Rig.app/Contents; \
	for lib in `otool -L MacOS/Rig | grep \.dylib | sed -e "s/ (.*//" | grep -Ev "^[[:space:]]*/(usr/lib|System)" `; do \
          libfn="`basename $$lib`"; \
          cp "$$lib" "Frameworks/$$libfn"; \
	  chmod 755 "Frameworks/$$libfn"; \
          install_name_tool -id "@executable_path/../Frameworks/$$libfn" "Frameworks/$$libfn"; \
          install_name_tool -change "$$lib" "@executable_path/../Frameworks/$$libfn" MacOS/Rig; \
	done
	@echo finishing up.
	@cp PkgInfo Rig.app/Contents
	@cp Info.plist Rig.app/Contents
	@cp Rig.icns Rig.app/Contents/Resources
