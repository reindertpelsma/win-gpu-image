@echo off
ver
echo WGI_GPU_BEGIN
set WGI_FAILED=0
set WGI_DRIVER_INF=
for /d %%D in (X:\Windows\System32\DriverStore\FileRepository\nv_dispi.inf_*) do set WGI_DRIVER_INF=%%D\nv_dispi.inf
if not defined WGI_DRIVER_INF goto missingdriver
drvload "%WGI_DRIVER_INF%"
echo DRVLOAD_RC=%errorlevel%
if errorlevel 1 set WGI_FAILED=1
goto smi
:missingdriver
echo NVIDIA_DRIVER_INF_MISSING
set WGI_FAILED=1
:smi
nvidia-smi.exe
echo NVIDIA_SMI_RC=%errorlevel%
if errorlevel 1 set WGI_FAILED=1
set WGI_TEST_DISK=
for %%D in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do if exist %%D:\WGI_TESTS.TAG set WGI_TEST_DISK=%%D:
if not defined WGI_TEST_DISK goto bundled
echo WGI_SECOND_DISK=%WGI_TEST_DISK%
if not exist "%WGI_TEST_DISK%\programs\d3d11-offscreen.exe" set WGI_FAILED=1
if not exist "%WGI_TEST_DISK%\programs\cuda-query.exe" set WGI_FAILED=1
if exist "%WGI_TEST_DISK%\programs\d3d11-offscreen.exe" call :run "%WGI_TEST_DISK%\programs\d3d11-offscreen.exe"
if exist "%WGI_TEST_DISK%\programs\cuda-query.exe" call :run "%WGI_TEST_DISK%\programs\cuda-query.exe"
goto done
:bundled
call :run X:\Windows\System32\d3d11-offscreen.exe
call :run X:\Windows\System32\cuda-query.exe
:done
if "%WGI_FAILED%"=="0" (echo WGI_GPU_PASS) else (echo WGI_GPU_FAIL)
echo WGI_GPU_END
exit /b 0
:run
%1
echo PROBE=%1 RC=%errorlevel%
if errorlevel 1 set WGI_FAILED=1
exit /b 0
