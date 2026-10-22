# Hypothesis: this ISO boots through wpeinit/startnet at the selected VM RAM size.
# Falsifier: QEMU exits or COM1 lacks WGI_PE_BOOT_OK_V1 within the deadline.
# Lower-memory trials measure the smallest tested boot size, not GPU sufficiency.
# No NVIDIA device is emulated: GPU binding and Vast test RAM remain unmeasured.
param([int]$TimeoutSeconds = 900, [int]$MemoryMiB = 8192, [switch]$MeasureMemoryFloor)
. "$PSScriptRoot/../scripts/common.ps1"
$root = (Get-Location).Path
$qemuRoot = [IO.Path]::GetFullPath("$root/work/qemu")
New-Item -ItemType Directory -Force $qemuRoot | Out-Null
Invoke-Installer "$root/cache/qemu.exe" "/S /D=$qemuRoot"
$qemu = "$qemuRoot/qemu-system-x86_64.exe"
Invoke-Checked $qemu @('--version')
$iso = "$root/out/image.iso"

function Test-BootAtMemory([int]$RamMiB) {
    $serial = "$root/work/serial-$RamMiB.log"
    $errlog = "$root/work/qemu-stderr-$RamMiB.log"
    $outlog = "$root/work/qemu-stdout-$RamMiB.log"
    if (Test-Path $serial) { Clear-Content $serial }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $arguments = @('-machine','q35','-accel','tcg,thread=multi','-cpu','max','-smp','2','-m',"$RamMiB",
        '-display','none','-monitor','none','-serial',"file:$serial",'-nic','none',
        '-no-reboot','-boot','d','-cdrom',$iso)
    # Start-Process joins arguments: quote each argument for local paths with spaces.
    $quoted = @($arguments | ForEach-Object { '"' + $_ + '"' })
    $process = Start-Process -FilePath $qemu -PassThru -RedirectStandardError $errlog -RedirectStandardOutput $outlog -ArgumentList $quoted
    $passed = $false
    $reason = 'serial marker deadline'
    $peakWorkingSet = 0L
    $peakPrivateBytes = 0L
    $minimumHostFreeKiB = [long]::MaxValue
    try {
        while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
            $process.Refresh()
            if ($process.HasExited) { $reason = "QEMU exit $($process.ExitCode)"; break }
            $peakWorkingSet = [math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
            $peakPrivateBytes = [math]::Max($peakPrivateBytes, $process.PrivateMemorySize64)
            $freeKiB = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory
            $minimumHostFreeKiB = [math]::Min($minimumHostFreeKiB, [long]$freeKiB)
            if ((Test-Path $serial) -and (Get-Content $serial -Raw) -match 'WGI_PE_BOOT_OK_V1') {
                $passed = $true; $reason = 'guest serial marker'; break
            }
            Start-Sleep -Seconds 2
        }
    } finally {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        if (Test-Path $errlog) { Get-Content $errlog -Tail 40 | Write-Host }
        if (Test-Path $serial) { Get-Content $serial -Tail 80 | Write-Host }
    }
    $result = [pscustomobject]@{
        memory_mib = $RamMiB; boot_pass = $passed; reason = $reason
        seconds = [math]::Round($watch.Elapsed.TotalSeconds,2)
        peak_qemu_working_set_bytes = $peakWorkingSet
        peak_sampled_qemu_private_bytes = $peakPrivateBytes
        minimum_host_free_kib = $minimumHostFreeKiB
        accelerator = 'TCG'; gpu_present = $false
    }
    Write-Host ('MEASURED QEMU_BOOT ' + ($result | ConvertTo-Json -Compress))
    return $result
}

$results = @()
$results += Test-BootAtMemory $MemoryMiB
if ($results[0].boot_pass -and $MeasureMemoryFloor) {
    foreach ($candidate in @(6144,4096) | Where-Object { $_ -lt $MemoryMiB }) {
        $results += Test-BootAtMemory $candidate
        if (!$results[-1].boot_pass) { break }
    }
}
$successes = @($results | Where-Object boot_pass)
$minimum = if ($successes.Count) { ($successes.memory_mib | Measure-Object -Minimum).Minimum } else { $null }
$measurements = @{
    trials = $results; minimum_tested_boot_memory_mib = $minimum
    gpu_test_memory_measured = $false
    vast_test_memory_mib_recommendation = 8192
    vast_recommendation_classification = 'INFERRED provisional lower bound; GPU driver binding/workload RAM unmeasured; no rented box used'
    limits = 'Boot-only minimum among tested sizes, not an exact hardware/GPU minimum; working set is not unique physical memory'
}
$measurements | ConvertTo-Json -Depth 8 | Set-Content out/smoke.json -Encoding utf8
Write-Host ('MEASURED QEMU_MEMORY_FLOOR ' + ($measurements | ConvertTo-Json -Depth 8 -Compress))
if ($env:GITHUB_STEP_SUMMARY) {
    @('', '## QEMU memory measurements', '', '| VM RAM MiB | Boot | Seconds | Peak QEMU working set bytes |', '| ---: | --- | ---: | ---: |') | Add-Content $env:GITHUB_STEP_SUMMARY
    $results | ForEach-Object { "| $($_.memory_mib) | $($_.boot_pass) | $($_.seconds) | $($_.peak_qemu_working_set_bytes) |" } | Add-Content $env:GITHUB_STEP_SUMMARY
    "`nSmallest successful tested boot allocation: $minimum MiB. Vast GPU test memory is unmeasured; provision at least 8192 MiB provisionally (INFERRED)." | Add-Content $env:GITHUB_STEP_SUMMARY
}
if (!$results[0].boot_pass) { throw "Primary QEMU smoke test failed at $MemoryMiB MiB: $($results[0].reason)" }
