@echo off
wpeinit
set "WGI_SMOKE="
for %%D in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do (
  if exist %%D:\WGI_RESULTS.TAG (
    >%%D:\wgi-boot.txt echo WGI_PE_BOOT_OK_V1
  )
  if exist %%D:\WGI_SMOKE.TAG set "WGI_SMOKE=1"
)
X:\Windows\System32\serial-log.exe WGI_PE_BOOT_OK_V1
call X:\Windows\System32\gpu-tests.cmd 2>&1 | X:\Windows\System32\serial-log.exe
if defined WGI_SMOKE wpeutil shutdown
:idle
ping -n 31 127.0.0.1 >nul
goto idle
