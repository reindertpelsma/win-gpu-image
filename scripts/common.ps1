$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $true

function Get-StoreReleaseForTag([string]$Json, [string]$Tag) {
    # gh --paginate --slurp returns one array per page. Flatten explicitly:
    # --slurp cannot be combined with --jq on the hosted runner's gh CLI.
    $pages = ConvertFrom-Json -InputObject $Json -NoEnumerate
    foreach ($page in $pages) {
        foreach ($release in $page) {
            if ($release.tag_name -eq $Tag) { $release }
        }
    }
}
function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$File exited $LASTEXITCODE" }
}
function Invoke-Installer([string]$File, [string]$Arguments) {
    $p = Start-Process -FilePath $File -ArgumentList $Arguments -Wait -PassThru
    if ($p.ExitCode -notin @(0,3010)) { throw "Installer $File exited $($p.ExitCode)" }
}
function Get-NvidiaDriverStoreMetrics([string]$FileRepository) {
    $repositories = @(Get-ChildItem -LiteralPath $FileRepository -Directory -Filter 'nv_dispi.inf_amd64_*')
    if ($repositories.Count -ne 1) { throw "Expected one NVIDIA display repository, found $($repositories.Count)" }
    # Enumerate a resolved literal directory: a wildcard -Path with -File filters
    # the directory itself and can produce an empty Measure-Object result.
    $files = @(Get-ChildItem -LiteralPath $repositories[0].FullName -File -Recurse)
    if (!$files.Count) { throw 'NVIDIA DriverStore repository is empty' }
    $bytes = 0L
    foreach ($file in $files) { $bytes += $file.Length }
    return [pscustomobject]@{ path = $repositories[0].FullName; files = $files.Count; bytes = $bytes }
}
