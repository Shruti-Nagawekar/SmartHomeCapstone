@echo off
echo Installing dependencies...
call npm install
echo.
echo Starting server...
echo Server will run on http://localhost:3000
echo Press Ctrl+C to stop
echo.
node server.js

