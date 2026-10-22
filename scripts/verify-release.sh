#!/usr/bin/env bash
set -euo pipefail
recipe_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
release_dir=$(cd -- "${1:?usage: verify-release.sh RELEASE_DIRECTORY}" && pwd)
(cd -- "$release_dir" && sha256sum -c SHA256SUMS)
python3 "$recipe_dir/recipe.py" verify "$release_dir"
python3 - "$release_dir" <<'PY'
import json, pathlib, shutil, sys
root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / 'manifest.json').read_text())
with (root / 'image.iso').open('wb') as out:
    for part in manifest['parts']:
        with (root / part['name']).open('rb') as src:
            shutil.copyfileobj(src, out, 8 * 1024**2)
print('Verified and reassembled', root / 'image.iso')
PY
