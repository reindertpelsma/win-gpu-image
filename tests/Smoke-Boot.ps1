# Hypothesis: this ISO reaches wpeinit/startnet and has the offline NVIDIA package.
# Falsifier: no exact guest-written result-disk marker by the deadline, or the
# offline DISM listing lacks the NVIDIA display driver. COM1 is optional logging.
# No NVIDIA device is emulated: GPU binding and Vast test RAM remain unmeasured.
param([int]$TimeoutSeconds = 900, [int]$MemoryMiB = 8192)
. "$PSScriptRoot/../scripts/common.ps1"
. "$PSScriptRoot/../scripts/Result-Disk.ps1"
$root = (Get-Location).Path
$driverListing = Get-Content "$root/out/drivers.txt" -Raw
if ($driverListing -notmatch 'Original File Name\s*:\s*nv_dispi\.inf' -or
    $driverListing -notmatch 'Class Name\s*:\s*Display' -or
    $driverListing -notmatch 'Provider Name\s*:\s*NVIDIA') {
    throw 'Offline DISM NVIDIA display driver assertion failed'
}
$qemuRoot = [IO.Path]::GetFullPath("$root/work/qemu")
New-Item -ItemType Directory -Force $qemuRoot | Out-Null
Invoke-Installer "$root/cache/qemu.exe" "/S /D=$qemuRoot"
$qemu = "$qemuRoot/qemu-system-x86_64.exe"
Invoke-Checked $qemu @('--version')
$serial = "$root/work/serial-$MemoryMiB.log"
$errlog = "$root/work/qemu-stderr-$MemoryMiB.log"
$outlog = "$root/work/qemu-stdout-$MemoryMiB.log"
if (Test-Path $serial) { Clear-Content $serial }
# A fresh disk is mandatory. The host never seeds wgi-boot.txt.
$resultDisk = New-SmokeResultDisk "$root/work/results-$MemoryMiB.vhd"
$arguments = @('-machine','q35','-accel','tcg,thread=multi','-cpu','max','-smp','2','-m',"$MemoryMiB",
    '-display','none','-monitor','none','-serial',"file:$serial",'-nic','none',
    '-no-reboot','-boot','d','-cdrom',"$root/out/image.iso",
    '-drive',"file=$resultDisk,format=vpc,if=ide,index=0")
# Start-Process joins arguments: quote each argument for local paths with spaces.
$quoted = @($arguments | ForEach-Object { '"' + $_ + '"' })
$watch = [Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath $qemu -PassThru -RedirectStandardError $errlog -RedirectStandardOutput $outlog -ArgumentList $quoted
$endReason = 'observation deadline; host stopped QEMU'
$peakWorkingSet = 0L
$peakPrivateBytes = 0L
$minimumHostFreeKiB = [long]::MaxValue
$diskEvidence = $null
try {
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $process.Refresh()
        if ($process.HasExited) { $endReason = "QEMU exit $($process.ExitCode)"; break }
        $peakWorkingSet = [math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
        $peakPrivateBytes = [math]::Max($peakPrivateBytes, $process.PrivateMemorySize64)
        $freeKiB = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory
        $minimumHostFreeKiB = [math]::Min($minimumHostFreeKiB, [long]$freeKiB)
        Start-Sleep -Seconds 2
    }
} finally {
    $watch.Stop()
    if (!$process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    if (Test-Path $errlog) { Get-Content $errlog -Tail 40 | Write-Host }
    if (Test-Path $serial) { Get-Content $serial -Tail 80 | Write-Host }
    # Never mount the VHD while QEMU has it open. Guest shutdown flushes the disk.
    $diskEvidence = Read-SmokeResultDisk $resultDisk
}
$passed = $diskEvidence.boot_marker_found -and !$diskEvidence.read_error
$reason = if ($passed) { 'exact guest result-disk marker and offline DISM driver check' } else { 'missing/invalid result-disk boot marker or disk read failure' }
$result = [pscustomobject]@{
    memory_mib = $MemoryMiB; boot_pass = $passed; reason = $reason
    observation_seconds = [math]::Round($watch.Elapsed.TotalSeconds,2)
    observation_end = $endReason; offline_nvidia_display_driver = $true
    peak_qemu_working_set_bytes = $peakWorkingSet
    peak_sampled_qemu_private_bytes = $peakPrivateBytes
    minimum_host_free_kib = $minimumHostFreeKiB
    accelerator = 'TCG'; gpu_present = $false
    result_disk_boot_marker_found = $diskEvidence.boot_marker_found
    result_disk_log_captured = $diskEvidence.log_captured
    serial_required = $false
}
$measurements = @{
    trials = @($result); tested_boot_memory_mib = $MemoryMiB
    minimum_boot_memory_measured = $false; gpu_test_memory_measured = $false
    vast_test_memory_mib_recommendation = 8192
    vast_recommendation_classification = 'INFERRED starting allocation, not a minimum; GPU driver/workload RAM unmeasured; no rented box used'
    limits = '8192 MiB is the tested default, not a minimum. Observation includes probe execution/shutdown, not exact boot time; host counters are not unique physical memory.'
}
$measurements | ConvertTo-Json -Depth 8 | Set-Content out/smoke.json -Encoding utf8
Write-Host ('MEASURED QEMU_BOOT ' + ($result | ConvertTo-Json -Compress))
if ($env:GITHUB_STEP_SUMMARY) {
    @('', '## QEMU boot and memory observations', '', '| VM RAM MiB | Result-disk boot gate | Observation seconds | Peak QEMU working set bytes |', '| ---: | --- | ---: | ---: |',
        "| $MemoryMiB | $passed | $($result.observation_seconds) | $peakWorkingSet |", '',
        '8192 MiB is the tested default, not a minimum. COM1 is best-effort only. Offline DISM NVIDIA display check passed. Observation includes probes/shutdown, not exact boot time. Vast GPU test RAM is unmeasured; 8192 MiB is an INFERRED starting allocation.') | Add-Content $env:GITHUB_STEP_SUMMARY
}
if (!$passed) { throw "QEMU result-disk smoke test failed at $MemoryMiB MiB: $reason ($endReason)" }
