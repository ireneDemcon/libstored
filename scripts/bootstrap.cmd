@echo off
set here=%~dp0
pushd %here%\..

net session > NUL 2> NUL
if errorlevel 1 goto no_admin
goto is_admin
:no_admin
echo Run this script as Administrator.
goto error
:is_admin

where /q choco > NUL 2> NUL
if errorlevel 1 goto no_choco
goto have_choco
:no_choco
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command " [System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
:choco_again
where /q choco > NUL 2> NUL
if errorlevel 1 goto still_no_choco
goto have_choco
:still_no_choco
echo Chocolatey not installed. Install from here: https://chocolatey.org/docs/installation
goto error
:have_choco

choco install -y --no-progress python3 tortoisegit git cmake make pkgconfiglite mingw doxygen.install plantuml
if errorlevel 1 goto error

where /q libcairo-2.dll > NUL 2> NUL
if errorlevel 1 goto no_cairo
goto have_cairo
:no_cairo
echo To build the documentation, you need the GTK+ runtime. Install it from here:
echo https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer/releases/download/2021-04-29/gtk3-runtime-3.24.29-2021-04-29-ts-win64.exe
rem sleep 5, to show the user this message
ping -n 6 127.0.0.1 > NUL
:have_cairo

call refreshenv

python.exe -m ensurepip
if errorlevel 1 goto error

python.exe -m pip install --upgrade setuptools
if errorlevel 1 goto error

python.exe -m pip install wheel
if errorlevel 1 goto error

python.exe -m pip install -r scripts\requirements.txt
if errorlevel 1 goto error

python.exe -m pip install PyQt5
if errorlevel 1 goto error

:done
popd
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
popd
pause
exit /b 1

