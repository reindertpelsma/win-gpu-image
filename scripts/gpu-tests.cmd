@echo off
rem The offline service is disabled. Hold the exact adapter before wpeinit can bind it.
set WGI_FAILED=0
X:\Windows\System32\gpu-prepare.exe --hold
set WGI_HOLD_RC=%errorlevel%
set WGI_NO_GPU=
if "%WGI_HOLD_RC%"=="3" set WGI_NO_GPU=1
if not "%WGI_HOLD_RC%"=="0" if not defined WGI_NO_GPU goto failed
wpeinit
if errorlevel 1 goto failed
rem This is a boot marker after wpeinit, not a GPU-success claim.
echo WGI_PE_BOOT_OK_V1
for %%D in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do (
  if exist %%D:\WGI_RESULTS.TAG >%%D:\wgi-boot.txt echo WGI_PE_BOOT_OK_V1
)
if defined WGI_NO_GPU goto failed
ver
echo WGI_GPU_BEGIN
set WGI_DRIVER_INF=
for /d %%D in (X:\Windows\System32\DriverStore\FileRepository\nv_dispi.inf_*) do set WGI_DRIVER_INF=%%D\nv_dispi.inf
if not defined WGI_DRIVER_INF goto failed
X:\Windows\System32\gpu-prepare.exe "%WGI_DRIVER_INF%"
if errorlevel 1 goto failed
nvidia-smi.exe
if errorlevel 1 goto failed
set WGI_TEST_DISK=
for %%D in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do if exist %%D:\WGI_TESTS.TAG set WGI_TEST_DISK=%%D:
if not defined WGI_TEST_DISK goto bundled
if not exist "%WGI_TEST_DISK%\programs\d3d11-offscreen.exe" goto failed
if not exist "%WGI_TEST_DISK%\programs\cuda-query.exe" goto failed
call :run "%WGI_TEST_DISK%\programs\d3d11-offscreen.exe"
call :run "%WGI_TEST_DISK%\programs\cuda-query.exe"
goto done
:bundled
call :run X:\Windows\System32\d3d11-offscreen.exe
call :run X:\Windows\System32\cuda-query.exe
:done
if not "%WGI_FAILED%"=="0" goto failed
echo WGI_GPU_PASS
echo WGI_GPU_END
exit /b 0
:failed
echo WGI_GPU_FAIL
echo WGI_GPU_END
exit /b 1
:run
%1
echo PROBE=%1 RC=%errorlevel%
if errorlevel 1 set WGI_FAILED=1
exit /b 0
