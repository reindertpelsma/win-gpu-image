# Boot evidence and diagnostics. Disk operations select a new task-owned VHD only.
. "$PSScriptRoot/common.ps1"
function Test-PeBootMarkerFile([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    return [IO.File]::ReadAllText($Path) -cmatch '\AWGI_PE_BOOT_OK_V1(?:\r?\n)?\z'
}
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
        # Only sentinels are seeded here; wgi-boot.txt must be written by the guest.
        [IO.File]::WriteAllText((Join-Path $driveRoot 'WGI_SMOKE.TAG'), "Shut down after CI probes`r`n")
    } finally {
        $image = Get-DiskImage -ImagePath $diskPath -ErrorAction SilentlyContinue
        if ($image -and $image.Attached) { Dismount-DiskImage -ImagePath $diskPath | Out-Null }
    }
    return $diskPath
}
function Write-PeDiagnosticLines([string]$Text, [string]$Label, [int]$MaxOutput) {
    $remaining = $MaxOutput - 512
    $truncated = $false
    foreach ($line in ($Text -split "`r?`n")) {
        # Prefix and escape data; raw guest/setupapi text cannot form workflow commands.
        $safe = [regex]::Replace($line, '[^\x20-\x7E\t]', {
            param($match) '\u{0:X4}' -f [int][char]$match.Value
        }).Replace('::', ': :')
        $rendered = "$Label $safe"
        if ($rendered.Length + 2 -gt $remaining) { $truncated = $true; break }
        Write-Host $rendered
        $remaining -= $rendered.Length + 2
    }
    return $truncated
}
function Read-PeDiagnosticFile([string]$Path, [string]$Label,
                               [ValidateRange(1024,262144)][int]$MaxBytes) {
    $stream = $null
    $captured = $false; $total = 0; $inputBytes = 0L; $errorText = $null
    $sourceTruncated = $false; $readTruncated = $false; $outputTruncated = $false
    $formatValid = $false
    try {
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            $stream = [IO.File]::OpenRead($Path)
            $inputBytes = $stream.Length
            $buffer = [byte[]]::new($MaxBytes)
            while ($total -lt $MaxBytes) {
                $n = $stream.Read($buffer, $total, $MaxBytes - $total)
                if (!$n) { break }
                $total += $n
            }
            $text = [Text.Encoding]::UTF8.GetString($buffer, 0, $total)
            $captured = $true
            $sourceTruncated = $text -cmatch '(?m)^WGI_DIAGNOSTIC_TRUNCATED(?:\r?\n|$)'
            $readTruncated = $inputBytes -gt $total
            $formatValid = $text -cmatch '\AWGI_'
            $outputTruncated = Write-PeDiagnosticLines $text $Label $MaxBytes
        } else { Write-Host "$Label MISSING" }
    } catch {
        $errorText = $_.Exception.GetType().Name
        Write-Host "$Label READ_ERROR type=$errorText"
    } finally { if ($stream) { $stream.Dispose() } }
    Write-Host ("$Label STATE bytes_read=$total input_bytes=$inputBytes " +
        "source_truncated=$sourceTruncated read_truncated=$readTruncated " +
        "output_truncated=$outputTruncated format_valid=$formatValid error=$errorText")
    return [pscustomobject]@{
        captured = $captured; bytes_read = $total; input_bytes = $inputBytes
        source_truncated = $sourceTruncated; read_truncated = $readTruncated
        output_truncated = $outputTruncated; format_valid = $formatValid; error = $errorText
    }
}
function Read-PeResultDirectory([string]$Root) {
    $marker = Test-PeBootMarkerFile (Join-Path $Root 'wgi-boot.txt')
    $receipt = Read-PeDiagnosticFile (Join-Path $Root 'wgi-serial-receipt.txt') `
        'GUEST_RECEIPT' 131072
    $runtime = Read-PeDiagnosticFile (Join-Path $Root 'wgi-runtime-setup.txt') `
        'GUEST_RUNTIME' 262144
    $log = Join-Path $Root 'wgi-results.txt'
    $captured = Test-Path -LiteralPath $log -PathType Leaf
    if ($captured) {
        Write-Host 'GUEST_RESULT_DISK_LOG_BEGIN'
        $tail = (Get-Content -LiteralPath $log -Tail 100) -join "`n"
        $truncated = Write-PeDiagnosticLines $tail 'GUEST_LOG' 65536
        Write-Host "GUEST_LOG STATE output_truncated=$truncated"
        Write-Host 'GUEST_RESULT_DISK_LOG_END'
    }
    return [pscustomobject]@{
        boot_marker_found = $marker; log_captured = $captured
        receipt = $receipt; runtime = $runtime
    }
}
function Read-SmokeResultDisk([string]$Path) {
    $captured = $false
    $marker = $false
    $readError = $null
    $receipt = $null; $runtime = $null
    try {
        Mount-DiskImage -ImagePath $Path | Out-Null
        $volumes = @(Get-DiskImage -ImagePath $Path | Get-Disk | Get-Partition |
            Get-Volume | Where-Object DriveLetter)
        foreach ($volume in $volumes) {
            $evidence = Read-PeResultDirectory ($volume.DriveLetter + ':\')
            if ($evidence.boot_marker_found) { $marker = $true }
            if ($evidence.log_captured) { $captured = $true }
            $receipt = $evidence.receipt; $runtime = $evidence.runtime
        }
    } catch {
        $readError = $_.Exception.Message
        Write-Host "RESULT_DISK_READ_FAILED $readError"
    } finally {
        $image = Get-DiskImage -ImagePath $Path -ErrorAction SilentlyContinue
        if ($image -and $image.Attached) { Dismount-DiskImage -ImagePath $Path | Out-Null }
    }
    return [pscustomobject]@{
        boot_marker_found = $marker; log_captured = $captured; read_error = $readError
        receipt = $receipt; runtime = $runtime
    }
}
