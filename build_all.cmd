@echo off

echo 1. COMPILING CLIENT UI
cd email_reader_client\ui
cmake .
cmake --build .
cd ../..

echo 2. COMPILING SERVER APP
del /f /q server.exe
cd server_app
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build .
cd ../..
copy "server_app\build\bin\Debug\server.exe" "server.exe"
