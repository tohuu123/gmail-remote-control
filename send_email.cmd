@echo off

@echo off
setlocal
pushd %~dp0

python -u "email_reader_client\src\sending.py"

popd
endlocal