start /WAIT %1 /SKIP_DIALOGS /AUTOSTART
IF ERRORLEVEL 1 ECHO %3 > %2
DEL %1
