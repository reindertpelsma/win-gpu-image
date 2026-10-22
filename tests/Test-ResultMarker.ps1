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
