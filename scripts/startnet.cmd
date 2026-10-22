@echo off
wpeinit
drvload X:\Windows\inf\msports.inf >X:\wgi-serial-driver.txt 2>&1
X:\Windows\System32\serial-log.exe WGI_PE_BOOT_OK_V1
type X:\wgi-serial-driver.txt | X:\Windows\System32\serial-log.exe
call X:\Windows\System32\gpu-tests.cmd 2>&1 | X:\Windows\System32\serial-log.exe
:idle
ping -n 31 127.0.0.1 >nul
goto idle
