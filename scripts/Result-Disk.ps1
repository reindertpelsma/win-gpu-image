# Diagnostic data disk only. All disk operations select a new task-owned VHD file.
. "$PSScriptRoot/common.ps1"
function New-SmokeResultDisk([string]$Path) {
    $diskPath = [IO.Path]::GetFullPath($Path)
    if (Test-Path -LiteralPath $diskPath) { throw 'Result VHD already exists; refuse to reuse it' }
    $letter = @('R','S','T','U','V','W') | Where-Object { $null -eq (Get-PSDrive -Name $_ -ErrorAction SilentlyContinue) } | Select-Object -First 1
    if (!$letter) { throw 'No free drive letter for the own result VHD' }
    $commandFile = $diskPath + '.diskpart.txt'
    $commands = @(
        "create vdisk file=`"$diskPath`" maximum=64 type=fixed",
        "select vdisk file=`"$diskPath`"", 'attach vdisk', 'convert mbr',
        'create partition primary', 'format fs=fat32 quick label=WGI_RESULTS',
        "assign letter=$letter", 'exit'
    )
    [IO.File]::WriteAllText($commandFile, ($commands -join "`r`n") + "`r`n")
    try {
        Invoke-Checked diskpart.exe @('/s',$commandFile) | Write-Host
        $driveRoot = $letter + ':\'
        if (!(Test-Path -LiteralPath $driveRoot)) { throw 'Own result VHD did not receive its drive letter' }
        [IO.File]::WriteAllText((Join-Path $driveRoot 'WGI_RESULTS.TAG'), "WGI smoke result disk`r`n")
    } finally {
        $image = Get-DiskImage -ImagePath $diskPath -ErrorAction SilentlyContinue
        if ($image -and $image.Attached) { Dismount-DiskImage -ImagePath $diskPath | Out-Null }
    }
    return $diskPath
}
function Read-SmokeResultDisk([string]$Path) {
    $captured = $false
    try {
        Mount-DiskImage -ImagePath $Path | Out-Null
        $volumes = @(Get-DiskImage -ImagePath $Path | Get-Disk | Get-Partition | Get-Volume | Where-Object DriveLetter)
        foreach ($volume in $volumes) {
            $log = ($volume.DriveLetter + ':\wgi-results.txt')
            if (Test-Path -LiteralPath $log) {
                Write-Host 'GUEST_RESULT_DISK_LOG_BEGIN'
                Get-Content -LiteralPath $log -Tail 100 | Write-Host
                Write-Host 'GUEST_RESULT_DISK_LOG_END'
                $captured = $true
            }
        }
    } catch { Write-Host "RESULT_DISK_DIAGNOSTIC_UNAVAILABLE $($_.Exception.Message)" }
    finally {
        $image = Get-DiskImage -ImagePath $Path -ErrorAction SilentlyContinue
        if ($image -and $image.Attached) { Dismount-DiskImage -ImagePath $Path | Out-Null }
    }
    return $captured
}
