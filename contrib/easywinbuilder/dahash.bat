@call %~dp0\set_vars.bat
@set FN=%1
@set FN=%FN:\=/%
@bash %~dp0\dahash.sh %FN%