@echo off
rem Serial opens before startup; its owned job bounds all startup/probe children.
X:\Windows\System32\serial-log.exe --run 240 ^
 "X:\Windows\System32\cmd.exe /d /c X:\Windows\System32\gpu-tests.cmd"
set WGI_SERIAL_RC=%errorlevel%
rem A serial transport failure is fatal, never rescued by the diagnostic disk.
if not "%WGI_SERIAL_RC%"=="0" echo WGI_TRANSPORT_OR_GPU_FAILED RC=%WGI_SERIAL_RC%
for %%D in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do (
  if exist %%D:\WGI_SMOKE.TAG wpeutil shutdown
)
:idle
ping -n 31 127.0.0.1 >nul
goto idle
