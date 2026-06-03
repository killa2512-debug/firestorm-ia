@echo off
cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
    echo.
    echo  ERROR: Python no esta instalado.
    echo  Instala Python 3 desde: https://www.python.org/downloads/
    echo  Asegurate de marcar "Add Python to PATH" al instalar.
    echo.
    pause
    exit /b 1
)

if not exist "%~dp0groq_config.ini" (
    python "%~dp0setup.py"
    if errorlevel 1 exit /b 1
)

start /b "groq-proxy" python "%~dp0groq-translate.py" 5000 1>>"%TEMP%\firestormia.log" 2>&1
ping 127.0.0.1 -n 3 >nul
start "" "%~dp0Firestorm-Releasex64.exe"
