# Hypothesis: this ISO reaches wpeinit/startnet and has the offline NVIDIA package.
# Falsifier: no exact guest-written result-disk marker by the deadline, or the
# offline DISM listing lacks the NVIDIA display driver, or actual COM1 protocol is missing.
# No NVIDIA device is emulated: GPU binding and Vast test RAM remain unmeasured.
param([int]$TimeoutSeconds = 900, [int]$MemoryMiB = 8192,
      [int]$FirstSerialSeconds = 600, [int]$TerminalSeconds = 250)
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
$firstSerial = $null
$firstTerminal = $null
$serialText = ''
function Observe-Serial {
    if (Test-Path $serial) { $script:serialText = Get-Content $serial -Raw }
    if ($null -eq $script:firstSerial -and
        $script:serialText -match '(?m)^WGI_PE_START_V2 terminal_budget_s=240\r?$') {
        $script:firstSerial = $watch.Elapsed.TotalSeconds
    }
    if ($null -eq $script:firstTerminal -and
        $script:serialText -match '(?m)^WGI_GPU_(FAIL|PASS)\r?$') {
        $script:firstTerminal = $watch.Elapsed.TotalSeconds
    }
}
try {
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Observe-Serial
        if ($null -eq $firstSerial -and $watch.Elapsed.TotalSeconds -gt $FirstSerialSeconds) {
            $endReason = 'missing bounded first COM1 readiness marker'; break
        }
        if ($null -ne $firstSerial -and $null -eq $firstTerminal -and
            $watch.Elapsed.TotalSeconds - $firstSerial -gt $TerminalSeconds) {
            $endReason = 'missing bounded COM1 terminal verdict'; break
        }
        $process.Refresh()
        if ($process.HasExited) { $endReason = "QEMU exit $($process.ExitCode)"; break }
        $peakWorkingSet = [math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
        $peakPrivateBytes = [math]::Max($peakPrivateBytes, $process.PrivateMemorySize64)
        $freeKiB = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory
        $minimumHostFreeKiB = [math]::Min($minimumHostFreeKiB, [long]$freeKiB)
        Start-Sleep -Seconds 2
    }
} finally {
    Observe-Serial
    $watch.Stop()
    if (!$process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    if (Test-Path $errlog) { Get-Content $errlog -Tail 40 | Write-Host }
    if (Test-Path $serial) { Get-Content $serial -Tail 80 | Write-Host }
    # Never mount the VHD while QEMU has it open. Guest shutdown flushes the disk.
    $diskEvidence = Read-SmokeResultDisk $resultDisk
}
$serial_boot_marker = $serialText -match '(?m)^WGI_PE_BOOT_OK_V1\r?$'
$serial_terminal_fail = $serialText -match '(?m)^WGI_GPU_FAIL\r?$'
$unexpected_gpu_pass = $serialText -match '(?m)^WGI_GPU_PASS\r?$'
$bounded = $null -ne $firstSerial -and $firstSerial -le $FirstSerialSeconds -and
    $null -ne $firstTerminal -and $firstTerminal - $firstSerial -le $TerminalSeconds
$passed = $diskEvidence.boot_marker_found -and !$diskEvidence.read_error -and
    $serial_boot_marker -and $serial_terminal_fail -and !$unexpected_gpu_pass -and $bounded
$reason = if ($passed) {
    'COM1 boot plus bounded no-GPU FAIL, result disk and offline driver check'
} else { 'missing/invalid COM1, bounds, no-GPU verdict or diagnostic disk evidence' }
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
    serial_required = $true
    serial_boot_marker = $serial_boot_marker; serial_terminal_fail = $serial_terminal_fail
    unexpected_gpu_pass = $unexpected_gpu_pass
    first_serial_seconds = $firstSerial; first_terminal_seconds = $firstTerminal
    first_serial_bound_seconds = $FirstSerialSeconds; terminal_bound_seconds = $TerminalSeconds
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
        '8192 MiB is the tested default, not a minimum.',
        'Actual COM1 boot and bounded explicit no-GPU FAIL are required.',
        'Offline DISM NVIDIA display check passed. Observation includes probes/shutdown.',
        'Vast GPU test RAM is unmeasured; 8192 MiB is an INFERRED starting allocation.') |
        Add-Content $env:GITHUB_STEP_SUMMARY
}
if (!$passed) { throw "QEMU serial/boot smoke test failed at $MemoryMiB MiB: $reason ($endReason)" }
