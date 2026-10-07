@echo off
rem Doble clic: graba solo (nivel, piloto y guion automaticos). Windows. / Double-click: records by itself.
cd /d "%~dp0\..\.."
where py >nul 2>nul && (py -3 tools\telemetry\record.py %*) || (where python >nul 2>nul && (python tools\telemetry\record.py %*) || (echo Instala Python 3 / Install Python 3: https://www.python.org/downloads/))
echo.
pause
