########################################################################################################
#
# To be compatible with the hamlib "rigctld" command, several files have been taken from
# the hamlib source code and slightly modified. These files are all from hamlib/tests:
#
# rigctl_parse.c
# rigctl_parse.h
# dumpcaps.c
# sprintflst.c
# sprintflst.h
# uthash.h
#
# To compile and link, hamlib must be installed on the machine. More specifically include files
# such as <hamlib/rig.h> must be found, and the argument "-lhamlib" in the link step must link
# with the corresponding hamlib library.
#
########################################################################################################

#
OBJS=		Rig.o Hamlib.o Winkey.o rigctl_parse.o dumpcaps.o sprintflst.o fmemopen.o
LIBS=		-lfltk -lportaudio -lhamlib -lpthread
CXX=		g++
CC=		gcc
CFLAGS=		-O
CXXFLAGS=	-O

#
# NOTE: "make Rig" makes the program
#       (MacOS, Linux)
#
Rig:	$(OBJS)
	$(CXX) -o Rig $(OBJS) $(LIBS)

rigctl_parse.o: rigctl_parse.c
	$(CC) $(CFLAGS) -c rigctl_parse.c

dumpcaps.o:
	$(CC) $(CFLAGS) -c dumpcaps.c

sprintflst.o:
	$(CC) $(CFLAGS) -c sprintflst.c

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
