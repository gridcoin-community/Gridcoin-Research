@set LANG=en_US.UTF8
@set LC_ALL=en_US.UTF8

@set MINGWINSTALLER=mingw-get-inst-20120426

@set OPENSSL=openssl-1.0.2l
@set BERKELEYDB=db-4.8.30.NC
@set BOOST=boost_1_64_0
@set BOOSTVERSION=1.64.0
@set LEVELDB=leveldb
@set BOOSTSUFFIX=-mgw53-mt-s-1_64
@set MINIUPNPC=miniupnpc-1.9
@set QRENCODE=qrencode-3.4.4
@set LIBPNG=lpng1629
@set ZLIB=zlib

@set EWBLIBS=libs

@set QTDIR=A:\Qt\Qt5.8.0
@set PATH=%QTDIR%\Tools\mingw530_32\bin;%QTDIR%\5.8\mingw53_32\bin;%PATH%
@set QMAKESPEC=%QTDIR%\5.8\mingw53_32\mkspecs\win32-g++

@set EWBPATH=contrib/easywinbuilder
set ROOTPATH=..\..
xset ROOTPATH=a:\gridcoin-research

@set ROOTPATHSH=%ROOTPATH:\=/%

@rem bootstrap coin name
@for /F %%a in ('dir /b %ROOTPATH%\*.pro') do @set COINNAME=%%a
@set COINNAME=%COINNAME:-qt.pro=%
@set COINNAME=gridcoinresearch


@set QTDOWNLOADPATH=http://download.qt-project.org/official_releases/qt/4.8/4.8.5/qt-win-opensource-4.8.5-mingw.exe
@rem Qt5 will need changes in gather_dlls.bat

rem @set MINGW=%QTMINGW%
@set MSYS=a:\msys\1.0\bin
@set PERL=a:/msys/1.0/bin/perl.exe
set PATH=%MSYS%;%PATH%

@rem the following will be set as additional CXXFLAGS and CFLAGS for everything - no ' or ", space is ok
@set ADDITIONALCCFLAGS=-fno-guess-branch-probability -frandom-seed=1984 -Wno-unused-variable -Wno-unused-value -Wno-sign-compare -Wno-strict-aliasing

@rem Note: Variables set here can NOT be overwritten in makefiles
