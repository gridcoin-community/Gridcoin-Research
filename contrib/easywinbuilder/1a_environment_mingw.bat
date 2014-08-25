@call set_vars.bat
@echo About to download MinGW installer - you need to install it manually like this:
@echo  Use prepackaged catalogue
@echo  Install to default directory: C:\MinGW
@echo  Configure options: C, C++, MSYS basic system
@echo.
@pause
@start http://sourceforge.net/projects/mingw/files/Installer/mingw-get-inst/%MINGWINSTALLER%/%MINGWINSTALLER%.exe/download
@set WAITMINGW=1
