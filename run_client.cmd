@echo off

echo 1. STARTING CLIENT UI
cd "email_reader_client\ui"
start "" "QT_TEST.exe"
cd ../..

echo 2. STARTING EMAIL READER
python -u "email_reader_client\src\read.py"

