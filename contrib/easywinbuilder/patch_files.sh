sed -i.bak 's/INCLUDEPATHS= \\/INCLUDEPATHS?= \\/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/LIBPATHS= \\/LIBPATHS?= \\/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/USE_UPNP:=-/USE_UPNP?=-/g' $ROOTPATHSH/src/makefile.mingw

sed -i.bak 's,#include <miniupnpc/miniwget.h>,#include <miniwget.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/miniupnpc.h>,#include <miniupnpc.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/upnpcommands.h>,#include <upnpcommands.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/upnperrors.h>,#include <upnperrors.h>,g' $ROOTPATHSH/src/net.cpp

sed -i.bak 's/\$(CC) -enable-stdcall-fixup/\$(CC) -Wl,-enable-stdcall-fixup/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw  # workaround, see http://stackoverflow.com/questions/13227354/warning-cannot-find-entry-symbol-nable-stdcall-fixup-defaulting
sed -i.bak 's/all:	init upnpc-static upnpc-shared testminixml libminiupnpc.a miniupnpc.dll/all:	init upnpc-static/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw  # only need static, rest is not compiling

# += does not work on windows defined variables
sed -i.bak 's/CFLAGS=-mthreads/CFLAGS=${ADDITIONALCCFLAGS} -mthreads/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/CC = gcc/CC=gcc ${ADDITIONALCCFLAGS} -Wall/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw

