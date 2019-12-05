OBJS=		Rig.o Hamlib.o Winkey.o rigctl_parse.o dumpcaps.o sprintflst.o fmemopen.o
LIBS=		-lfltk -lportaudio -lhamlib -lreadline -lpthread
CXX=		g++
CC=		gcc
CFLAGS=		-O
CXXFLAGS=	-O
HAMLIB=		../hamlib

#
# NOTE: "make Rig" makes the program
#       (MacOS, Linux)
#
Rig:	$(OBJS)
	$(CXX) -o Rig $(OBJS) $(LIBS)

#
# NOTE: we use original files from the hamlib repository, instead of
#       copying them here. Hamlib must reside in ../hamlib
#       we use the following files:
#
# tests/dumpcaps.c
# tests/sprintflst.c
# tests/sprintflst.h
# tests/rigctl_parse.c
# tests/rigctl_parse.h
# tests/uthash.h
#
# src/misc.h
# src/iofunc.h
# src/serial.h
#
 
rigctl_parse.o:
	$(CC) $(CFLAGS) -c -I../hamlib/src -I../hamlib/tests -o $@ ../hamlib/tests/rigctl_parse.c


dumpcaps.o:
	$(CC) $(CFLAGS) -c -I../hamlib/src -I../hamlib/tests -o $@ ../hamlib/tests/dumpcaps.c

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
