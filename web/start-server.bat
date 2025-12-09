@echo off
echo Starting web server...
echo Open http://localhost:8000 in your browser
echo Press Ctrl+C to stop
cd /d %~dp0
python -m http.server 8000

