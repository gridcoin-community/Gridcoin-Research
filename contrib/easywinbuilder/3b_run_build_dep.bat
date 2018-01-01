@call set_vars.bat
@bash ./build_dep.sh
@if not "%RUNALL%"=="1" pause
