@call set_vars.bat
copy %QTPATH%\Qt5Charts.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Core.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Gui.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Network.dll %ROOTPATH%\release\
copy %QTPATH%\Qt5Widgets.dll %ROOTPATH%\release\
copy %QTPATH%\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy %QTPATH%\libstdc++-6.dll %ROOTPATH%\release\
copy %QTPATH%\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy %QTPATH%\libwinpthread-1.dll %ROOTPATH%\release\
@if not "%RUNALL%"=="1" pause
