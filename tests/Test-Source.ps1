# CPU-only build: no vendor input, image, GPU, environment or store access.
$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
python -m unittest discover -s tests -v
if ($LASTEXITCODE -ne 0) { throw 'CPU recipe tests failed' }
./tests/Test-DriverMetrics.ps1
./tests/Test-ResultMarker.ps1
./tests/Test-StoreReleases.ps1
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio x64 C++ tools are required' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('wgi-source-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $scratch | Out-Null
try {
    $lines = @('@echo off', "call `"$devcmd`" -arch=x64 -host_arch=x64",
        'if errorlevel 1 exit /b 1')
    foreach ($source in Get-ChildItem programs -File | Where-Object Extension -in '.c', '.cpp') {
        $name = $source.BaseName
        $language = if ($source.Extension -eq '.cpp') { '/EHsc /std:c++17' } else { '/std:c11' }
        $lines += "cl /nologo /O2 /MT /W4 $language /D_CRT_SECURE_NO_WARNINGS " +
            "/Fe:`"$scratch/$name.exe`" /Fo:`"$scratch/$name.obj`" `"$($source.FullName)`" " +
            '/link d3d11.lib dxgi.lib setupapi.lib cfgmgr32.lib advapi32.lib'
        $lines += 'if errorlevel 1 exit /b 1'
    }
    $lines += 'exit /b 0'
    $script = Join-Path $scratch 'compile.cmd'
    [IO.File]::WriteAllText($script, ($lines -join "`r`n"), [Text.Encoding]::ASCII)
    & cmd.exe /d /c $script
    if ($LASTEXITCODE -ne 0) { throw 'Original Windows probe compilation failed' }
    Write-Host 'WGI_SOURCE_BUILD_OK CPU_ONLY=1 GPU_EXECUTION=0'
} finally {
    [IO.Directory]::Delete($scratch, $true)
}
