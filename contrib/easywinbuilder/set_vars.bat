@set PATH=a:\mingw\msys\1.0\bin;a:\mingw\bin

@set LANG=en_US.UTF8
@set LC_ALL=en_US.UTF8

@set MINGWINSTALLER=mingw-get-inst-20120426

@set OPENSSL=openssl-1.0.1e
@set BERKELEYDB=db-4.8.30.NC
@set BOOST=boost_1_55_0
@set BOOSTVERSION=1.55.0
@set LEVELDB=leveldb
@rem If you wonder why there is no -s- see: https://github.com/bitcoin/bitcoin/pull/2835#issuecomment-21231694
@set BOOSTSUFFIX=-mgw46-mt-1_55
@set MINIUPNPC=miniupnpc-1.9

@set EWBLIBS=libs


set QTDIR=A:\Qt\4.8.4
set QMAKESPEC=A:\Qt\4.8.4\mkspecs\win32-g++
set PATH=%PATH%;a:\qt\4.8.4\qmake\bin
@set QTPATH=a:\Qt\4.8.4\bin

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

@set MSYS=a:/MinGW/msys/1.0/bin
@set PERL=%MSYS%/perl.exe

@rem the following will be set as additional CXXFLAGS and CFLAGS for everything - no ' or ", space is ok
@set ADDITIONALCCFLAGS=-fno-guess-branch-probability -frandom-seed=1984 -Wno-unused-variable -Wno-unused-value -Wno-sign-compare -Wno-strict-aliasing

@rem Note: Variables set here can NOT be overwritten in makefiles