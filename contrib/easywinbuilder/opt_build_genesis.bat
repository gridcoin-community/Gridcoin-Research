@call set_vars.bat
@bash patch_files.sh
@echo Building Bitcoin daemon...
@rem todo: rewrite this with ^ line wrapping
@set PARAMS=BOOST_SUFFIX=%BOOSTSUFFIX%
@set PARAMS=%PARAMS% INCLUDEPATHS="
@rem set PARAMS=%PARAMS%-I'../src'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%BOOST%'
@set PARAMS=%PARAMS% -I'../%EWBLIBS%/%OPENSSL%/include'
@set PARAMS=%PARAMS%"
@set PARAMS=%PARAMS% LIBPATHS="
@set PARAMS=%PARAMS% -L'../%EWBLIBS%/%OPENSSL%'
@set PARAMS=%PARAMS%"
@set PARAMS=%PARAMS% ADDITIONALCCFLAGS="%ADDITIONALCCFLAGS%"
@set PARAMS=%PARAMS:\=/%
@echo PARAMS: %PARAMS%

@set PARAMS=%PARAMS% USE_UPNP=1
@rem remove "rem " from the next line to deactivate upnp
@rem set PARAMS=%PARAMS% USE_UPNP=-

@cd %ROOTPATH%\genesis

g++ -c -o genesis.o genesis.cpp -lcrypto -I ../libs/openssl-1.0.1e/include -fpermissive


@mingw32-make -f makefile.mingw %PARAMS% -lcrypto
@if errorlevel 1 goto error
@echo.
@echo.
@strip genesis.exe
@if errorlevel 1 goto error
@echo !!!!!!! DONE: Find genesis.exe in ./genesis     :)
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
