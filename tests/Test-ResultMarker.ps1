# Hypothesis: only an exact guest boot marker passes; sentinels/logs are insufficient.
# Falsifier: a missing file, wrong version, prefix/suffix, or case change passes.
. "$PSScriptRoot/../scripts/Result-Disk.ps1"
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('wgi-marker-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $scratch | Out-Null
try {
    $path = Join-Path $scratch 'wgi-boot.txt'
    if (Test-PeBootMarkerFile $path) { throw 'Absent marker accepted' }
    foreach ($bad in @('', 'WGI_RESULTS.TAG', 'WGI_PE_BOOT_OK_V0', 'wgi_pe_boot_ok_v1', 'prefix WGI_PE_BOOT_OK_V1', "WGI_PE_BOOT_OK_V1`r`nextra")) {
        [IO.File]::WriteAllText($path, $bad)
        if (Test-PeBootMarkerFile $path) { throw "Invalid marker accepted: $bad" }
    }
    foreach ($good in @('WGI_PE_BOOT_OK_V1', "WGI_PE_BOOT_OK_V1`r`n", "WGI_PE_BOOT_OK_V1`n")) {
        [IO.File]::WriteAllText($path, $good)
        if (!(Test-PeBootMarkerFile $path)) { throw 'Valid marker rejected' }
    }
    Write-Host 'RESULT_MARKER_TEST_OK missing_and_corrupt_rejected=true'
} finally { [IO.Directory]::Delete($scratch, $true) }

# Exercise the actual directory collector using only new task-owned text fixtures.
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('wgi-receipt-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $scratch | Out-Null
$script:capturedLines = [Collections.Generic.List[string]]::new()
function Write-Host([object]$Object) { $script:capturedLines.Add([string]$Object) }
try {
    [IO.File]::WriteAllText((Join-Path $scratch 'wgi-boot.txt'), "WGI_PE_BOOT_OK_V1`r`n")
    $noise = ((1..1000 | ForEach-Object { "OFFLINE_NOISE_$_" }) -join "`n")
    [IO.File]::WriteAllText((Join-Path $scratch 'wgi-results.txt'), $noise)
    $receipt = Join-Path $scratch 'wgi-serial-receipt.txt'
    [IO.File]::WriteAllText($receipt,
        "WGI_SERIAL_SERVICE_STATE state=4`nWGI_UART problem=28 service=UNKNOWN`n")
    $runtime = Join-Path $scratch 'wgi-runtime-setup.txt'
    [IO.File]::WriteAllText($runtime, "wpeinit runtime fixture`n::error::not a workflow command`n")
    $result = Read-PeResultDirectory $scratch
    if (!$result.boot_marker_found -or !$result.receipt.captured -or !$result.runtime.captured) {
        throw 'Actual collector did not retain separate own diagnostic files'
    }
    $printed = $script:capturedLines -join "`n"
    if ($printed -notmatch 'GUEST_RECEIPT WGI_UART problem=28') {
        throw 'Structured UART receipt was displaced by generic log tail'
    }
    if ($printed -notmatch 'GUEST_RUNTIME wpeinit runtime fixture') {
        throw 'Independent runtime setup evidence was lost'
    }
    if ($printed -match '(?m)^::error::' -or $printed -match '::error::not') {
        throw 'Untrusted log could form a workflow command'
    }
    [IO.File]::WriteAllText($receipt, ('z' * 200000))
    $r = Read-PeDiagnosticFile $receipt 'GUEST_RECEIPT' 131072
    if (!$r.read_truncated -or $r.bytes_read -gt 131072 -or !$r.output_truncated -or
        $r.format_valid) {
        throw 'Oversized/malformed receipt did not retain explicit bounded-read state'
    }
    [IO.File]::WriteAllText($receipt, "WGI_DIAGNOSTIC_TRUNCATED`r`n")
    if (!(Read-PeDiagnosticFile $receipt 'GUEST_RECEIPT' 131072).source_truncated) {
        throw 'Guest-side censoring marker was ignored'
    }
    [IO.File]::WriteAllBytes($receipt, [byte[]](0,27,10,255,58,58,101,114,114,111,114,58,58))
    $script:capturedLines.Clear()
    $r = Read-PeDiagnosticFile $receipt 'GUEST_RECEIPT' 131072
    $printed = $script:capturedLines -join "`n"
    if ($r.format_valid -or $printed.Contains([string][char]0) -or
        $printed.Contains([string][char]27) -or $printed -match '::error::') {
        throw 'Malformed/control bytes were not explicitly invalid and safely escaped'
    }
    $locked = [IO.File]::Open($receipt, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite,
                              [IO.FileShare]::None)
    try {
        $r = Read-PeDiagnosticFile $receipt 'GUEST_RECEIPT' 131072
        if ($r.captured -or !$r.error) { throw 'Actual file read error was hidden' }
    } finally { $locked.Dispose() }
    Remove-Item -LiteralPath $receipt
    if ((Read-PeDiagnosticFile $receipt 'GUEST_RECEIPT' 131072).captured) {
        throw 'Missing receipt falsely claimed capture'
    }
    # Mock disk/volume resolution only: the actual collector still reads own fixture files.
    $script:actualDirectoryReader = (Get-Command Read-PeResultDirectory).ScriptBlock
    $script:fixtureRoot = $scratch
    $script:mounted = 0; $script:unmounted = 0
    function Mount-DiskImage { $script:mounted++ }
    function Get-DiskImage { [pscustomobject]@{ Attached = $true } }
    function Get-Disk { process { $_ } }
    function Get-Partition { process { $_ } }
    function Get-Volume { process { [pscustomobject]@{ DriveLetter = 'Z' } } }
    function Dismount-DiskImage { $script:unmounted++ }
    function Read-PeResultDirectory([string]$Root) {
        if ($Root -ne 'Z:\') { throw 'Mock own-volume root was not used' }
        & $script:actualDirectoryReader $script:fixtureRoot
    }
    $result = Read-SmokeResultDisk 'own-mocked.vhd'
    if (!$result.boot_marker_found -or $result.read_error -or
        $script:mounted -ne 1 -or $script:unmounted -ne 1) {
        throw 'Actual mounted-disk collector or retirement failed under own-volume mock'
    }
    [Console]::WriteLine('RESULT_RECEIPT_TEST_OK own_files=1 real_disk_operations=0')
} finally {
    Remove-Item Function:Write-Host -ErrorAction SilentlyContinue
    [IO.Directory]::Delete($scratch, $true)
}
