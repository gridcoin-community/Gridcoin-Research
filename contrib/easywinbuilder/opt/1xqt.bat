@call set_vars.bat
@echo Building QT


set INCLUDE=a:\Gridcoin-Research\libs\libpng;a:\Gridcoin-Research\libs\openssl-1.0.1g\include
set LIB=a:\Gridcoin-Research\libs\libpng\.libs;a:\Gridcoin-Research\libs\openssl-1.0.1g

set PATH=%PATH%;a:\Qt\5.8.0\bin;c:\windows\system32;a:\MinGW32\bin;a:\perl\bin
cd a:\Qt\5.8.0
configure.bat -release -opensource -confirm-license -static -make libs -no-sql-sqlite -no-opengl -system-zlib -qt-pcre -no-icu -no-gif -system-libpng -no-libjpeg -no-freetype -no-angle -openssl -no-dbus -no-qml-debug

mingw32-make
@pause



cd C:\Qt\qttools-opensource-src-5.8.0
qmake qttools.pro
mingw32-make


@pause
