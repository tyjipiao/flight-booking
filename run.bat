@echo off
cd /d "%~dp0"
if not exist "bin\FlightBooking.exe" (
  echo [ERROR] bin\FlightBooking.exe not found. Run build.bat first.
  pause & exit /b 1
)
"bin\FlightBooking.exe"
pause