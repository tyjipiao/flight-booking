@echo off
rem ============================================================
rem  Flight Booking System - one-click build
rem  Auto-detects the MSVC toolchain and the MySQL client library.
rem  Override with environment variables when detection fails:
rem     set VCVARS64=<full path to vcvars64.bat>
rem     set MYSQL_ROOT=<folder containing include\mysql.h and lib\libmysql.lib>
rem ============================================================
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "OUT=%ROOT%bin"
set "SRC=%ROOT%src"
set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"

echo ============================================================
echo   Flight Booking System - build
echo ============================================================

rem ---------- 1) locate vcvars64.bat ----------
set "VCVARS="
if defined VCVARS64 if exist "%VCVARS64%" set "VCVARS=%VCVARS64%"
if not defined VCVARS if exist "%ROOT%tools\vcvars64.bat" set "VCVARS=%ROOT%tools\vcvars64.bat"

if not defined VCVARS (
  if exist "!PF86!\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`"!PF86!\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
      if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)

if not defined VCVARS (
  for %%b in ("!PF!\Microsoft Visual Studio" "!PF86!\Microsoft Visual Studio") do (
    if exist "%%~b" (
      for /d %%v in ("%%~b\*") do (
        for /d %%e in ("%%v\*") do (
          if exist "%%e\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%e\VC\Auxiliary\Build\vcvars64.bat"
        )
      )
    )
  )
)

if not defined VCVARS (
  echo [ERROR] Visual Studio C++ build tools not found.
  echo         Install "Build Tools for Visual Studio" with the
  echo         "Desktop development with C++" workload, or set VCVARS64.
  pause & exit /b 1
)

rem ---------- 2) locate MySQL client (include\mysql.h + lib\libmysql.lib) ----------
set "MYSQL_ROOT="
if defined MYSQL_DIR if exist "%MYSQL_DIR%\include\mysql.h" set "MYSQL_ROOT=%MYSQL_DIR%"

if not defined MYSQL_ROOT (
  for %%r in (
    "!PF!\MySQL\MySQL Server 8.0"
    "!PF!\MySQL\MySQL Server 8.4"
    "!PF!\MySQL\MySQL Server 9.0"
    "!PF86!\MySQL\MySQL Server 8.0"
    "!PF86!\MySQL\MySQL Server 8.4"
    "C:\MySQL\MySQL Server 8.0"
    "D:\MySQL\MySQL Server 8.0"
    "D:\MySQL\mysql-8.0"
  ) do (
    if not defined MYSQL_ROOT if exist "%%~r\include\mysql.h" set "MYSQL_ROOT=%%~r"
  )
)

if not defined MYSQL_ROOT (
  for %%d in (C D E F) do (
    if exist "%%d:\" (
      for /d %%m in ("%%d:\Program Files\MySQL\MySQL Server *") do (
        if not defined MYSQL_ROOT if exist "%%m\include\mysql.h" set "MYSQL_ROOT=%%m"
      )
      for /d %%m in ("%%d:\MySQL\MySQL Server *") do (
        if not defined MYSQL_ROOT if exist "%%m\include\mysql.h" set "MYSQL_ROOT=%%m"
      )
      for /d %%m in ("%%d:\mysql\mysql-*") do (
        if not defined MYSQL_ROOT if exist "%%m\include\mysql.h" set "MYSQL_ROOT=%%m"
      )
    )
  )
)

if not defined MYSQL_ROOT (
  echo [ERROR] MySQL client headers not found ^(include\mysql.h + lib\libmysql.lib^).
  echo         They ship with MySQL Server 8.0+, not with MySQL Shell/Workbench.
  echo         Set MYSQL_ROOT to your server folder, e.g.
  echo             set MYSQL_ROOT=C:\Program Files\MySQL\MySQL Server 8.0
  pause & exit /b 1
)

echo [1/3] vcvars64 : !VCVARS!
echo [1/3] MySQL    : !MYSQL_ROOT!

if not exist "%OUT%" mkdir "%OUT%"

echo [2/3] Compiling...
call "!VCVARS!" >nul
if errorlevel 1 (
  echo [ERROR] vcvars64.bat failed to initialize the MSVC environment.
  pause & exit /b 1
)

cl /nologo /EHsc /std:c++17 /utf-8 /O2 /W3 /I "!MYSQL_ROOT!\include" /Fo"%OUT%\\" /Fe"%OUT%\FlightBooking.exe" "%SRC%\main.cpp" "%SRC%\service.cpp" /link /LIBPATH:"!MYSQL_ROOT!\lib" libmysql.lib
if errorlevel 1 (
  echo.
  echo [ERROR] Build failed. Send the compiler output back for help.
  pause & exit /b 1
)

echo [3/3] Copying runtime DLLs and config...
for %%d in (libmysql.dll) do (
  if exist "!MYSQL_ROOT!\lib\%%d" copy /y "!MYSQL_ROOT!\lib\%%d" "%OUT%" >nul
)
for %%d in (libcrypto-3-x64.dll libssl-3-x64.dll) do (
  if exist "!MYSQL_ROOT!\bin\%%d" copy /y "!MYSQL_ROOT!\bin\%%d" "%OUT%" >nul
)

if exist "%ROOT%config.ini" (
  copy /y "%ROOT%config.ini" "%OUT%" >nul
) else (
  copy /y "%ROOT%config.example.ini" "%OUT%\config.ini" >nul
  echo.
  echo [WARN] config.ini not found in the project root.
  echo        Copied config.example.ini to bin\config.ini - fill in your DB
  echo        password before running, or create config.ini in the project root.
)

echo.
echo [OK] %OUT%\FlightBooking.exe
echo Run sql\schema.sql in MySQL Workbench first if you have not already.
pause