. "$PSScriptRoot/common.ps1"
$adk = "${env:ProgramFiles(x86)}/Windows Kits/10/Assessment and Deployment Kit"
Write-Host "MEASURED UTC=$([DateTime]::UtcNow.ToString('o')) ADK_PREINSTALLED=$(Test-Path $adk)"
Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,TotalVisibleMemorySize,FreePhysicalMemory | Format-List
Get-PSDrive -PSProvider FileSystem | Select-Object Name,Used,Free | Format-Table
(Get-Item "$env:SystemRoot/System32/dism.exe").VersionInfo | Select-Object FileVersion | Format-List
Write-Host "MEASURED QEMU_PREINSTALLED=$([bool](Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue))"
Get-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform | Select-Object FeatureName,State | Format-List
Get-CimInstance Win32_ComputerSystem | Select-Object HypervisorPresent | Format-List
