@echo off
wpeinit
X:\Windows\System32\serial-log.exe WGI_PE_BOOT_OK_V1
call X:\Windows\System32\gpu-tests.cmd 2>&1 | X:\Windows\System32\serial-log.exe
:idle
ping -n 31 127.0.0.1 >nul
goto idle
