########################################################################################################
#
# This is hamlib's rigctld (Rig Control Daemon) augmented by a graphical user interface.
# Therefore, the hamlib repository is "cloned" and installed in the sub-dir hamlib-local.
#
# Note hamlib is linked statically so there can be no conflict with some hamlib version already
# present in the system
#
########################################################################################################

HAMLIB_INCLUDE=	-Ihamlib-local/include -Ihamlib-local/src -Ihamlib-local/tests
HAMLIB_LIB=	hamlib-local/src/.libs/libhamlib.a

#
OBJS=		Rig.o Hamlib.o Winkey.o rigctl_parse.o dumpcaps.o
LIBS=		-lfltk -lportaudio -lpthread
CFLAGS?=	-O
CXXFLAGS?=	-O

RigCtl:	$(OBJS)
	$(CXX) -o RigCtl $(OBJS) $(HAMLIB_LIB) $(LIBS) $(LDFLAGS)

Hamlib.o:	Hamlib.c hamlib-local
	$(CC) $(CFLAGS) $(HAMLIB_INCLUDE) -c Hamlib.c

rigctl_parse.o: hamlib-local/tests/rigctl_parse.c
	$(CC) $(CFLAGS) $(HAMLIB_INCLUDE) -c hamlib-local/tests/rigctl_parse.c -o rigctl_parse.o

dumpcaps.o: hamlib-local/tests/dumpcaps.c
	$(CC) $(CFLAGS) $(HAMLIB_INCLUDE) -c hamlib-local/tests/dumpcaps.c -o dumpcaps.o

hamlib-local/tests/dumpcaps.c hamlib-local/tests/rigctl_parse.c hamlib-local:
	./build-hamlib.sh

clean:
	rm -rf $(OBJS) RigCtl RigCtl.app

realclean:
	rm -rf $(OBJS) RigCtl RigCtl.app hamlib-local

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
