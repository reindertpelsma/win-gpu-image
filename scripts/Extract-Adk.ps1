param([string]$MsiPath, [string]$Destination, [string]$SevenZip)
. "$PSScriptRoot/common.ps1"
# Read-only Windows Installer database queries; no installer/custom actions run.
$installer = New-Object -ComObject WindowsInstaller.Installer
$database = $installer.OpenDatabase($MsiPath, 0)
$tables = @{}
$definitions = @{
    Directory = @('Directory','Directory_Parent','DefaultDir')
    Component = @('Component','Directory_')
    File = @('File','Component_','FileName','FileSize','Sequence')
    Media = @('LastSequence','Cabinet')
}
foreach ($table in $definitions.Keys) {
    $columns = $definitions[$table]
    $quoted = @($columns | ForEach-Object { '``'.Substring(0,1) + $_ + '``'.Substring(0,1) })
    $query = 'SELECT ' + ($quoted -join ',') + ' FROM ' + '``'.Substring(0,1) + $table + '``'.Substring(0,1)
    $view = $database.OpenView($query)
    $view.Execute()
    $rows = @()
    while ($null -ne ($record = $view.Fetch())) {
        $row = @{}
        for ($i = 0; $i -lt $columns.Count; $i++) { $row[$columns[$i]] = $record.StringData($i + 1) }
        $rows += $row
    }
    $view.Close()
    $tables[$table] = $rows
}
$metadata = Join-Path (Get-Location).Path 'work/msi-tables.json'
$tables | ConvertTo-Json -Depth 8 | Set-Content $metadata -Encoding utf8
Invoke-Checked python @('scripts/extract_msi.py',$metadata,(Split-Path $MsiPath -Parent),$Destination,$SevenZip)
