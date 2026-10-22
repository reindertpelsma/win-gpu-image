# Hypothesis: resolved DriverStore enumeration counts nested payload files only.
# Falsifier: the wildcard-directory regression gives an empty result, or another
# driver's bytes leak into the NVIDIA measurement.
. "$PSScriptRoot/../scripts/common.ps1"
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('wgi-metrics-' + [Guid]::NewGuid().ToString('N'))
try {
    $driver = Join-Path $scratch 'nv_dispi.inf_amd64_fixture'
    $nested = Join-Path $driver 'NvCamera'
    $other = Join-Path $scratch 'other.inf_amd64_fixture'
    New-Item -ItemType Directory -Force $nested,$other | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $driver 'nvlddmkm.sys'), [byte[]]@(1,2,3,4,5))
    [IO.File]::WriteAllBytes((Join-Path $nested 'camera.dll'), [byte[]]@(1,2,3,4,5,6,7))
    [IO.File]::WriteAllBytes((Join-Path $other 'unrelated.sys'), [byte[]]@(1,2,3))
    $result = Get-NvidiaDriverStoreMetrics $scratch
    if ($result.files -ne 2 -or $result.bytes -ne 12) { throw 'DriverStore byte-count regression' }
    Write-Host 'DRIVERSTORE_METRICS_TEST_OK files=2 bytes=12 unrelated_excluded=true'
} finally {
    if (Test-Path -LiteralPath $scratch) { [IO.Directory]::Delete($scratch, $true) }
}
