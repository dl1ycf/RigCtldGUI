########################################################################################################
#
# To be compatible with the hamlib "rigctld" command, several files have been taken from
# the hamlib source code (unmodified!).
#
# rigctl_parse.c
# rigctl_parse.h
# dumpcaps.c
# sprintflst.c
# sprintflst.h
# uthash.h
#
# To be able to use them, we have provided the following header files and
# included there only the bare minimum required to compile those files.
#
# misc.h
# iofunc.h
# serial.h
#
# Do not overwrite these three files with files from the hamlib repository!
#
# To compile and link, hamlib must be installed on the machine. More specifically include files
# such as <hamlib/rig.h> must be found, and the argument "-lhamlib" in the link step must link
# with the corresponding hamlib library.
#
########################################################################################################

#
OBJS=		Rig.o Hamlib.o Winkey.o rigctl_parse.o dumpcaps.o sprintflst.o
LIBS=		-lfltk -lportaudio -lhamlib -lpthread
CFLAGS?=	-O
CXXFLAGS?=	-O

#
# NOTE: "make Rig" makes the program
#       (MacOS, Linux)
#
RigCtl:	$(OBJS)
	$(CXX) -o RigCtl $(OBJS) $(LIBS) $(LDFLAGS)

rigctl_parse.o: rigctl_parse.c
	$(CC) $(CFLAGS) -c rigctl_parse.c

dumpcaps.o:
	$(CC) $(CFLAGS) -c dumpcaps.c

sprintflst.o:
	$(CC) $(CFLAGS) -c sprintflst.c

clean:
	rm -rf $(OBJS) RigCtl RigCtl.app

####################################################################
#
# NOTE: "make app" is for Macintosh OSX only
#       Note that the libraries are NOT put into the app bundle
#       except for hamlib
#       PkgInfo, Info.plist, Rig.icns
#
####################################################################
app:  RigCtl
	@echo Creating AppBundle ....
	@rm -rf RigCtl.app
	@mkdir RigCtl.app
	@mkdir -p RigCtl.app/Contents/MacOS
	@mkdir -p RigCtl.app/Contents/Resources
	@mkdir -p RigCtl.app/Contents/Frameworks
	@cp RigCtl RigCtl.app/Contents/MacOS/RigCtl
	@cp PkgInfo RigCtl.app/Contents
	@cp Info.plist RigCtl.app/Contents
	@cp RigCtl.icns RigCtl.app/Contents/Resources
#
#       Copy hamlib into the executable because this is a library that we have
#       compiled and installed ourself.
#
	lib=`otool -L RigCtl.app/Contents/MacOS/RigCtl | grep libhamlib | sed -e "s/ (.*//" | sed -e "s/	//" `; \
	 libfn=`basename $$lib`; \
         cp "$$lib" "RigCtl.app/Contents/Frameworks/$$libfn"; \
         chmod u+w "RigCtl.app/Contents/Frameworks/$$libfn"; \
         install_name_tool -id "@executable_path/../Frameworks/$$libfn" "RigCtl.app/Contents/Frameworks/$$libfn"; \
         install_name_tool -change "$$lib" "@executable_path/../Frameworks/$$libfn" RigCtl.app/Contents/MacOS/RigCtl

