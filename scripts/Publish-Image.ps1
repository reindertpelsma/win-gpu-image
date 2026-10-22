. "$PSScriptRoot/common.ps1"
if (!$env:GH_TOKEN) { throw 'Private-store publish credential is unavailable in this environment' }
$store = 'reindertpelsma/win-gpu-image-private-store'
$manifest = Get-Content release/manifest.json -Raw | ConvertFrom-Json
$tag = 'img-' + $manifest.image_sha256.Substring(0,16)
Invoke-Checked python @('scripts/recipe.py','verify','release')
# List failures are fatal; a network/authorization error is not treated as absent.
$releases = & gh api --paginate --slurp "repos/$store/releases?per_page=100"
if ($LASTEXITCODE -ne 0) { throw 'Cannot list private-store releases' }
$existing = @(Get-StoreReleaseForTag ($releases -join "`n") $tag)
if ($existing.Count) {
    if ($existing[0].draft) { throw "Release $tag is an incomplete draft; owner must inspect it before retrying" }
    Write-Host "Release $tag already exists; skipping"
    exit 0
}
$notes = "$($manifest.image_sha256)`n$($manifest.run_url)`nhttps://github.com/reindertpelsma/win-gpu-image/commit/$($manifest.repo_commit)`n"
$notePath = Join-Path $env:TEMP 'wgi-release-notes.txt'
[IO.File]::WriteAllText($notePath, $notes)
$assets = @(Get-ChildItem release -File | Sort-Object Name | ForEach-Object FullName)
# Draft creation avoids exposing an incomplete release if an upload fails.
Invoke-Checked gh (@('release','create',$tag,'--repo',$store,'--draft','--title',$tag,'--notes-file',$notePath) + $assets)
Invoke-Checked gh @('release','edit',$tag,'--repo',$store,'--draft=false')
Write-Host "PUBLISHED https://github.com/$store/releases/tag/$tag"
