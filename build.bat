@echo off
rem ============================================================
rem  Flight Booking System - one-click build
rem  Toolchain: Visual Studio Build Tools 2026 + MySQL 8.0 client lib
rem ============================================================
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "MYSQL_ROOT=D:\Program Files\MySQL\MySQL Server 8.0"
set "OUT=%~dp0bin"
set "SRC=%~dp0src"

if not exist "%VCVARS%" (
  echo [ERROR] vcvars64.bat not found. Check your Visual Studio Build Tools path.
  pause & exit /b 1
)
if not exist "%MYSQL_ROOT%\include\mysql.h" (
  echo [ERROR] mysql.h not found under "%MYSQL_ROOT%".
  pause & exit /b 1
)

if not exist "%OUT%" mkdir "%OUT%"

echo [1/3] Setting up MSVC environment...
call "%VCVARS%" >nul
if errorlevel 1 (
  echo [ERROR] vcvars64.bat failed.
  pause & exit /b 1
)

echo [2/3] Compiling...
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /W3 /I "%MYSQL_ROOT%\include" /Fo"%OUT%\\" /Fe"%OUT%\FlightBooking.exe" "%SRC%\main.cpp" "%SRC%\service.cpp" /link /LIBPATH:"%MYSQL_ROOT%\lib" libmysql.lib
if errorlevel 1 (
  echo.
  echo [ERROR] Build failed. Send the compiler output back for help.
  pause & exit /b 1
)

echo [3/3] Copying runtime DLLs and config...
copy /y "%MYSQL_ROOT%\lib\libmysql.dll" "%OUT%" >nul
copy /y "%MYSQL_ROOT%\bin\libcrypto-3-x64.dll" "%OUT%" >nul
copy /y "%MYSQL_ROOT%\bin\libssl-3-x64.dll" "%OUT%" >nul

if exist "%~dp0config.ini" (
  copy /y "%~dp0config.ini" "%OUT%" >nul
) else (
  copy /y "%~dp0config.example.ini" "%OUT%\config.ini" >nul
  echo.
  echo [WARN] config.ini not found in project root.
  echo        Copied config.example.ini as bin\config.ini - fill in your DB password
  echo        before running, or create config.ini in the project root.
)

echo.
echo [OK] %OUT%\FlightBooking.exe
echo Run sql\schema.sql in MySQL Workbench first if you have not already.
pause