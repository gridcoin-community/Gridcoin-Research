@call set_vars.bat
@if not "%WAITQT%" == "1" goto continue
@echo Ensure Qt installer has finished.
@pause
:continue
@bash patch_files.sh
@cd %ROOTPATH%



@echo building qt - qmake...
@set COINNAME=Gridcoin

@set QMPS=BOOST_INCLUDE_PATH=%EWBLIBS%/%BOOST%^
 BOOST_LIB_PATH=%EWBLIBS%/%BOOST%/stage/lib^
 BOOST_LIB_SUFFIX=%BOOSTSUFFIX%^
 OPENSSL_INCLUDE_PATH=%EWBLIBS%/%OPENSSL%/include^
 OPENSSL_LIB_PATH=%EWBLIBS%/%OPENSSL%^
 BDB_INCLUDE_PATH=%EWBLIBS%/%BERKELEYDB%/build_unix^
 BDB_LIB_PATH=%EWBLIBS%/%BERKELEYDB%/build_unix^
 CURL_INCLUDE_PATH=%EWBLIBS%/curl^
 CURL_LIB_PATH=%EWBLIBS%/curl/bin^
 LIBZIP_INCLUDE_PATH=%EWBLIBS%/libzip/include^
 LIBZIP_LIB_PATH=%EWBLIBS%/libzip/lib^
 MINIUPNPC_INCLUDE_PATH=%EWBLIBS%/%MINIUPNPC%^
 MINIUPNPC_LIB_PATH=%EWBLIBS%/%MINIUPNPC%^
 QRENCODE_INCLUDE_PATH=%EWBLIBS%/%QRENCODE%^
 QRENCODE_LIB_PATH=%EWBLIBS%/%QRENCODE%/.libs^
 QMAKE_CXXFLAGS="%ADDITIONALCCFLAGS%"^
 QMAKE_CFLAGS="%ADDITIONALCCFLAGS%"

@qmake.exe %QMPS% USE_QRCODE=1 ZZZ=1 
@echo.
@echo.
@echo building qt - make...



mingw32-make -f Makefile.Release
@if errorlevel 1 goto continue
@cd %ROOTPATH%





@echo !!!!!!! %COINNAME%-qt DONE: Find %COINNAME%-qt.exe in ./release :)
:continue
@echo.
@echo.
@echo.
@echo.
@cd %EWBPATH%
@if not "%RUNALL%"=="1" pause
