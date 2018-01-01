@call set_vars.bat
copy %QTPATH%\Qt5Charts.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Core.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Gui.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Network.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Widgets.dll %ROOTPATH%\release\
copy %MINGW%\bin\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy %MINGW%\bin\libstdc++-6.dll %ROOTPATH%\release\
copy %MINGW%\bin\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy %MINGW%\bin\libwinpthread-1.dll %ROOTPATH%\release\
@if not "%RUNALL%"=="1" pause
