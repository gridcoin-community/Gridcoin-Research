@call set_vars.bat
copy %QTDIR%\5.8\mingw53_32\bin\Qt5Charts.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\Qt5Core.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\Qt5Gui.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\Qt5Network.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\Qt5Widgets.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy "%QTDIR%\5.8\mingw53_32\bin\libstdc++-6.dll" %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\bin\libwinpthread-1.dll %ROOTPATH%\release\
copy %QTDIR%\5.8\mingw53_32\plugins\iconengines\qsvgicon.dll %ROOTPATH%\release\iconengines\qsvgicon.dll
copy %QTDIR%\5.8\mingw53_32\plugins\imageformats\qsvg.dll %ROOTPATH%\release\imageformats\qsvg.dll
copy %QTDIR%\5.8\mingw53_32\plugins\platforms\qwindows.dll %ROOTPATH%\release\platforms\qwindows.dll
copy %QTDIR%\Tools\mingw530_32\opt\bin\libeay32.dll %ROOTPATH%\release\libeay32.dll
copy %QTDIR%\Tools\mingw530_32\opt\bin\ssleay32.dll %ROOTPATH%\release\ssleay32.dll
@if not "%RUNALL%"=="1" pause
