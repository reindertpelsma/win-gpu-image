# Hypothesis: a tag match on any API page retains the exact release/draft state.
# Falsifier: empty pages produce a match, a different tag matches, or later pages
# are lost and allow an existing published/draft release to be recreated.
. "$PSScriptRoot/../scripts/common.ps1"
$tag = 'img-0123456789abcdef'
if (@(Get-StoreReleaseForTag '[[]]' $tag).Count) { throw 'Empty store matched a tag' }
$pages = '[[{"tag_name":"img-other","draft":false}],[],[{"tag_name":"img-0123456789abcdef","draft":true}]]'
$found = @(Get-StoreReleaseForTag $pages $tag)
if ($found.Count -ne 1 -or !$found[0].draft -or $found[0].tag_name -ne $tag) { throw 'Later-page draft match was lost' }
$published = $pages.Replace('"draft":true', '"draft":false')
$found = @(Get-StoreReleaseForTag $published $tag)
if ($found.Count -ne 1 -or $found[0].draft) { throw 'Published release state changed' }
Write-Host 'STORE_RELEASE_PAGINATION_TEST_OK empty_and_later_pages=true draft_state_preserved=true'
