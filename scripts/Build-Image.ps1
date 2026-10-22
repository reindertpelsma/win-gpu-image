. "$PSScriptRoot/common.ps1"
New-Item -ItemType Directory -Force work,work/adk,work/mount,work/media,out,work/programs | Out-Null
$root = (Get-Location).Path
$seven = "$env:ProgramFiles/7-Zip/7z.exe"
if (!(Test-Path $seven)) { throw 'Install 7-Zip first' }

# Administrative extraction uses only the pinned offline MSI/cabinet layout.
# It avoids changing/reusing an unknown preinstalled ADK version.
$msis = @(Get-ChildItem cache/adk/Installers -Filter '*.msi' | Where-Object Name -match '^(Windows Deployment Image Servicing and Management Tools|Oscdimg) \(')
$msis += @(Get-ChildItem cache/winpe/Installers -Filter '*.msi' | Where-Object Name -match '^Windows PE (Boot Files|wims) \(')
if ($msis.Count -ne 7) { throw "Expected seven ADK/PE MSI packages, found $($msis.Count)" }
foreach ($msi in $msis) {
    Write-Host "ADMIN_EXTRACT $($msi.Name)"
    Invoke-Installer msiexec.exe "/a `"$($msi.FullName)`" /qn TARGETDIR=`"$root/work/adk`" /l*v `"$root/work/msi.log`""
}
$dism = @(Get-ChildItem work/adk -Recurse -Filter dism.exe | Where-Object FullName -match '[\\/]amd64[\\/]DISM[\\/]dism.exe$')
if ($dism.Count -ne 1) { throw "Expected one pinned x64 DISM, found $($dism.Count)" }
$script:DismExe = $dism[0].FullName
Write-Host "MEASURED BUILD_DISM=$($dism[0].VersionInfo.FileVersion)"
function Dism([string[]]$DismArgs) { Invoke-Checked $script:DismExe ($DismArgs + @('/English',"/LogPath:$root/work/dism.log")) }
$pe = @(Get-ChildItem work/adk -Recurse -Filter winpe.wim | Where-Object FullName -match '[\\/]amd64[\\/]en-us[\\/]winpe.wim$')
if ($pe.Count -ne 1) { throw 'Pinned amd64 en-us WinPE WIM not found' }
$peRoot = Split-Path (Split-Path $pe[0].FullName -Parent) -Parent
Copy-Item "$peRoot/Media/*" work/media -Recurse -Force
New-Item -ItemType Directory -Force work/media/sources | Out-Null
Copy-Item $pe[0].FullName work/base.wim
(Get-Item work/base.wim).IsReadOnly = $false

# Extract only display files. Never run the vendor installer.
Invoke-Checked $seven @('x','-y','-bd',"-o$root/work/nvidia",'cache/nvidia.exe','Display.Driver/*')
Invoke-Checked python @('scripts/recipe.py','stage-driver','work/nvidia/Display.Driver','work/driver','out/sizes.json')

# Use MSVC and the runner SDK; all in-repo program source is original.
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio x64 C++ tools are required' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$compile = @"
@echo off
call "$devcmd" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Fe:work/programs/serial-log.exe /Fo:work/programs/serial-log.obj programs/serial-log.c
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 /std:c11 /Fe:work/programs/cuda-query.exe /Fo:work/programs/cuda-query.obj programs/cuda-query.c
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 /Fe:work/programs/d3d11-offscreen.exe /Fo:work/programs/d3d11-offscreen.obj programs/d3d11-offscreen.cpp /link d3d11.lib dxgi.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText("$root/work/compile.cmd", $compile.Replace("`n","`r`n"))
Invoke-Checked cmd.exe @('/d','/c',"$root/work/compile.cmd")

# Obtain Microsoft graphics runtime DLLs directly from the pinned evaluation ISO.
Invoke-Checked $seven @('x','-y','-bd',"-o$root/work/eval",'cache/win11-ltsc-eval.iso','sources/install.wim')
New-Item -ItemType Directory -Force work/eval-mount | Out-Null
Dism @('/Mount-Image',"/ImageFile:$root/work/eval/sources/install.wim",'/Index:1',"/MountDir:$root/work/eval-mount",'/ReadOnly')
try {
    foreach ($name in @('d3d11.dll','dxgi.dll','DXCore.dll','D3DCompiler_47.dll','D3D12.dll','D3D12Core.dll','d3d10warp.dll','d3d11on12.dll')) {
        Copy-Item "work/eval-mount/Windows/System32/$name" work/programs -Force
    }
} finally { Dism @('/Unmount-Image',"/MountDir:$root/work/eval-mount",'/Discard') }

Dism @('/Mount-Image',"/ImageFile:$root/work/base.wim",'/Index:1',"/MountDir:$root/work/mount")
$committed = $false
try {
    Dism @("/Image:$root/work/mount",'/Add-Driver',"/Driver:$root/work/driver/nv_dispi.inf")
    $listing = & $script:DismExe "/Image:$root/work/mount" /Get-Drivers /Format:List /English
    if ($LASTEXITCODE -ne 0) { throw 'DISM get-drivers failed' }
    $listing | Set-Content out/drivers.txt
    $text = $listing -join "`n"
    if ($text -notmatch 'Original File Name\s*:\s*nv_dispi.inf' -or $text -notmatch 'Class Name\s*:\s*Display' -or $text -notmatch 'Provider Name\s*:\s*NVIDIA') {
        throw 'Offline NVIDIA display driver assertion failed'
    }
    Write-Host 'OFFLINE_NVIDIA_DISPLAY_DRIVER_OK'
    $listing | Write-Host
    Dism @("/Image:$root/work/mount",'/Set-ScratchSpace:512')
    Dism @("/Image:$root/work/mount",'/Get-ScratchSpace')
    $system32 = "$root/work/mount/Windows/System32"
    Copy-Item work/programs/*.exe,work/programs/*.dll $system32 -Force
    Copy-Item work/driver/nvidia-smi.exe,work/driver/nvml.dll $system32 -Force
    foreach ($name in @('startnet.cmd','gpu-tests.cmd')) {
        $content = (Get-Content "scripts/$name" -Raw).Replace("`r`n","`n").Replace("`n","`r`n")
        [IO.File]::WriteAllText("$system32/$name", $content, [Text.Encoding]::ASCII)
    }
    [IO.File]::WriteAllText("$system32/winpeshl.ini", "[LaunchApps]`r`n%SYSTEMROOT%\System32\cmd.exe, /c X:\Windows\System32\startnet.cmd`r`n", [Text.Encoding]::ASCII)
    # Serial service normally enumerates through wpeinit; assert actual files exist.
    foreach ($driver in @('serial.sys','serenum.sys')) {
        if (!(Test-Path "$system32/drivers/$driver")) { throw "PE missing $driver" }
    }
    $repoBytes = (Get-ChildItem "$system32/DriverStore/FileRepository/nv_dispi*" -File -Recurse | Measure-Object Length -Sum).Sum
    Write-Host "MEASURED OFFLINE_DRIVERSTORE_BYTES=$repoBytes"
    Dism @('/Unmount-Image',"/MountDir:$root/work/mount",'/Commit')
    $committed = $true
} finally {
    if (!$committed) { Dism @('/Unmount-Image',"/MountDir:$root/work/mount",'/Discard') }
}
Dism @('/Export-Image',"/SourceImageFile:$root/work/base.wim",'/SourceIndex:1',"/DestinationImageFile:$root/work/media/sources/boot.wim",'/Compress:max','/CheckIntegrity')
$oscdimg = @(Get-ChildItem work/adk -Recurse -Filter oscdimg.exe | Where-Object FullName -match '[\\/]amd64[\\/]') | Select-Object -First 1
$etfs = @(Get-ChildItem work/adk -Recurse -Filter etfsboot.com | Where-Object FullName -match '[\\/]amd64[\\/]') | Select-Object -First 1
$efi = @(Get-ChildItem work/adk -Recurse -Filter efisys_noprompt.bin | Where-Object FullName -match '[\\/]amd64[\\/]') | Select-Object -First 1
if (!$oscdimg -or !$etfs -or !$efi) { throw 'Pinned boot ISO tooling not found' }
$boot = "-bootdata:2#p0,e,b$($etfs.FullName)#pEF,e,b$($efi.FullName)"
Invoke-Checked $oscdimg.FullName @('-m','-o','-u2','-udfver102','-h',$boot,"$root/work/media","$root/out/image.iso")
Write-Host "MEASURED BOOT_WIM_BYTES=$((Get-Item work/media/sources/boot.wim).Length) IMAGE_BYTES=$((Get-Item out/image.iso).Length)"
