@echo off
rem Bootstrap is diagnostic-only: its success is not a GPU or serial verdict.
if exist X:\wgi-bootstrap-ok.txt del X:\wgi-bootstrap-ok.txt
X:\Windows\System32\gpu-prepare.exe --hold
set WGI_HOLD_RC=%errorlevel%
set WGI_NO_GPU=
if "%WGI_HOLD_RC%"=="3" set WGI_NO_GPU=1
if not "%WGI_HOLD_RC%"=="0" if not defined WGI_NO_GPU goto failed
rem The exact adapter is held, and offline nvlddmkm Start4 remains in force.
wpeinit
if errorlevel 1 goto failed
>X:\wgi-bootstrap-ok.txt echo WGI_BOOTSTRAP_OK_V1
echo WGI_BOOTSTRAP_OK_V1
exit /b 0
:failed
echo WGI_BOOTSTRAP_FAILED
exit /b 1
