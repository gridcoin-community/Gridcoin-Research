@echo off
::  Helper script to build BDB products for binary release
::  Assumes current directory is <product>/dist
::  Current version of Visual Studio for binaries is 2005.
::  This is enforced but if testing is desired for other releases
::  it's possible to cut/paste.
::

:: Try to find different versions of Visual Studio

call :TryBat "%VS80COMNTOOLS%\vsvars32.bat" && goto VS80
call :TryBat "%VS90COMNTOOLS%\vsvars32.bat" && goto VS90

:: no luck
goto batnotfound

:VS80
echo Using Visual Studio format 8.0 (Visual Studio 2005)
@echo "" > winbld.out
@echo devenv /build Release ..\build_windows\Berkeley_DB.sln
@devenv /build Release ..\build_windows\Berkeley_DB.sln >> winbld.out
@echo devenv /build Release ..\build_windows\Berkeley_DB.sln /project VS8\db_java.vcproj
@devenv /build Release ..\build_windows\Berkeley_DB.sln /project VS8\db_java.vcproj
@echo devenv /build Release ..\build_windows\BDB_dotNet.sln
@devenv /build Release ..\build_windows\BDB_dotNet.sln >> winbld.out
@echo devenv /build Release ..\build_windows\VS8\db_sql_jdbc.vcproj
@devenv /build Release ..\build_windows\VS8\db_sql_jdbc.vcproj >> winbld.out
@echo devenv /build Release ..\build_windows\VS8\db_sql_odbc.vcproj
@devenv /build Release ..\build_windows\VS8\db_sql_odbc.vcproj >> winbld.out
goto :eof

:VS90
@echo Windows binaries must be built using Visual Studio 2005
@echo If you want to test VS2008, uncomment the goto line below.
@echo Note these build lines will not work with the express editions
@echo of VS which do not appear to include "devenv."
@echo Express editions can be built with lines like:
@echo "VCExpress Berkeley_DB.sln /build" or
@echo "VCSExpress BDB_dotNet.sln /build"
goto :batnotfound
@echo Using Visual Studio format 9.0 (Visual Studio 2008)
@echo "" > winbld.out
@echo devenv /build Release ..\build_windows\Berkeley_DB.sln
@devenv /build Release ..\build_windows\Berkeley_DB.sln >> winbld.out
@echo devenv /build Release ..\build_windows\Berkeley_DB.sln /project VS8\db_java.vcproj
@devenv /build Release ..\build_windows\Berkeley_DB.sln /project VS8\db_java.vcproj
@echo devenv /build Release ..\build_windows\BDB_dotNet.sln
@devenv /build Release ..\build_windows\BDB_dotNet.sln >> winbld.out
@echo devenv /build Release ..\build_windows\VS8\db_sql_jdbc.vcproj
@devenv /build Release ..\build_windows\VS8\db_sql_jdbc.vcproj >> winbld.out
@echo devenv /build Release ..\build_windows\VS8\db_sql_odbc.vcproj
@devenv /build Release ..\build_windows\VS8\db_sql_odbc.vcproj >> winbld.out
goto :eof

:batnotfound
echo *********** ERROR: Visual Studio Config batch file not found *************
exit 3
goto end

:: TryBat(BATPATH)
:: If the BATPATH exists, use it and return 0,
:: otherwise, return 1.

:TryBat
:: Filename = "%1"
if not exist %1 exit /b 1
call %1
exit /b 0
goto :eof

:end
