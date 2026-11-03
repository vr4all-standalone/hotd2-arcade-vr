param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build-win/vrtest/reticle-controls'
python -B (Join-Path $PSScriptRoot 'reticle-settings-test.py') $repo $out
if($LASTEXITCODE){exit $LASTEXITCODE}
python -B (Join-Path $PSScriptRoot 'reticle-gl-test.py') $repo $out
exit $LASTEXITCODE
