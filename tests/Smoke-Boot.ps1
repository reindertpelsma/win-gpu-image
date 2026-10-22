# Hypothesis: this freshly generated ISO boots through wpeinit and startnet in QEMU.
# Falsifier: QEMU exits or COM1 lacks WGI_PE_BOOT_OK_V1 after 900 seconds.
# No NVIDIA device is emulated: this cannot establish driver binding/GPU correctness.
param([int]$TimeoutSeconds = 900)
. "$PSScriptRoot/../scripts/common.ps1"
$root = (Get-Location).Path
$qemuRoot = "$root/work/qemu"
New-Item -ItemType Directory -Force $qemuRoot | Out-Null
Invoke-Installer "$root/cache/qemu.exe" "/S /D=$qemuRoot"
$qemu = "$qemuRoot/qemu-system-x86_64.exe"
Invoke-Checked $qemu @('--version')
$serial = "$root/work/serial.log"
$errlog = "$root/work/qemu-stderr.log"
$outlog = "$root/work/qemu-stdout.log"
$iso = "$root/out/image.iso"
$watch = [Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath $qemu -PassThru -RedirectStandardError $errlog -RedirectStandardOutput $outlog -ArgumentList @(
    '-machine','q35','-accel','tcg,thread=multi','-cpu','max','-smp','2','-m','6144',
    '-display','none','-monitor','none','-serial',"file:$serial",'-nic','none',
    '-no-reboot','-boot','d','-cdrom',$iso
)
try {
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path $serial) -and (Get-Content $serial -Raw) -match 'WGI_PE_BOOT_OK_V1') {
            Write-Host "MEASURED QEMU_BOOT_PASS seconds=$([math]::Round($watch.Elapsed.TotalSeconds,2)) accelerator=TCG memory_mib=6144"
            Get-Content $serial | Write-Host
            exit 0
        }
        $process.Refresh()
        if ($process.HasExited) { throw "QEMU exited $($process.ExitCode) before guest marker" }
        Start-Sleep -Seconds 2
    }
    throw "No serial boot marker after $TimeoutSeconds seconds"
} finally {
    if (!$process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    if (Test-Path $errlog) { Get-Content $errlog -Tail 40 | Write-Host }
    if (Test-Path $serial) { Get-Content $serial -Tail 80 | Write-Host }
}
