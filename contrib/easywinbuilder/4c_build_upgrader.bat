@set COINNAME=gridcoinresearch
@call set_vars.bat
@bash patch_files.sh
@echo Building Gridcoin daemon...
@rem todo: rewrite this with ^ line wrapping

@set PARAMS=BOOST_SUFFIX=%BOOSTSUFFIX%
@set PARAMS=%PARAMS% INCLUDEPATHS="
@rem set PARAMS=%PARAMS%-I'../src'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%BOOST%'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%OPENSSL%/include'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%BERKELEYDB%/build_unix'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%LEVELDB%/include'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%MINIUPNPC%'
@set PARAMS=%PARAMS% -I'A:\Gridcoin-Research\libs\libzip\lib'
@set PARAMS=%PARAMS% -I'A:\Gridcoin-Research\libs\curl\include'
@set PARAMS=%PARAMS% -I'../%QTPATH%/'
@set PARAMS=%PARAMS%"
@set PARAMS=%PARAMS% LIBPATHS="
@set PARAMS=%PARAMS%-L'../src/leveldb'
@set PARAMS=%PARAMS% -L'../%EWBLIBS%/%BOOST%/stage/lib'
@set PARAMS=%PARAMS% -L'../%EWBLIBS%/%OPENSSL%'
@set PARAMS=%PARAMS% -L'../%EWBLIBS%/%BERKELEYDB%/build_unix'
@set PARAMS=%PARAMS% -L'../%EWBLIBS%/%MINIUPNPC%'
@set PARAMS=%PARAMS% -L'a:\Gridcoin-Research\libs\curl\include'
@set PARAMS=%PARAMS% -L'a:\Gridcoin-Research\libs\libzip\lib'
@set PARAMS=%PARAMS% -L'../%QTPATH%/'
@set PARAMS=%PARAMS%"
@set PARAMS=%PARAMS% ADDITIONALCCFLAGS="%ADDITIONALCCFLAGS%"
@set PARAMS=%PARAMS:\=/%
@echo PARAMS: %PARAMS%

@rem set PARAMS=%PARAMS% USE_UPNP=1
@rem remove "rem " from the next line to deactivate upnp
@set PARAMS=%PARAMS% USE_UPNP=-

@set PARAMS=%PARAMS% USE_DEVDIR=1

@cd %ROOTPATH%\src


@mingw32-make -f makefile_upgrader.mingw %PARAMS%
@if errorlevel 1 goto error
@echo.
@echo.
@strip %COINNAME%d.exe
@if errorlevel 1 goto error
@echo !!!!!!! %COINNAME% daemon DONE: Find %COINNAME%d.exe in ./src :)
@echo.
@echo.
@if not "%RUNALL%"=="1" pause
@goto end

:error
@echo.
@echo.
@echo !!!!!! Error! Build daemon failed.
@pause
:end
@cd ..\%EWBPATH%
