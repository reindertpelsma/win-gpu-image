. "$PSScriptRoot/common.ps1"
Invoke-Checked python @('scripts/inputs.py','fetch')
if ((Get-Content cache/first-use.txt -Raw).Trim() -eq 'true') {
    foreach ($item in @(@('adk','adksetup.exe','OptionId.DeploymentTools'), @('winpe','adkwinpesetup.exe','OptionId.WindowsPreinstallationEnvironment'))) {
        $layout = (Resolve-Path "cache/$($item[0])").Path
        Invoke-Installer "$layout/$($item[1])" "/quiet /layout `"$layout`" /features $($item[2]) /ceip off /log `"$env:TEMP/wgi-$($item[0])-layout.log`""
    }
    Invoke-Checked python @('scripts/inputs.py','record-layouts')
    throw 'FIRST USE: commit the WGI_INPUT metadata as inputs.lock.json, then rerun. No image was built or published.'
}
