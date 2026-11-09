param(
    [string]$OutputDirectory = ('artifacts\releases\isolated-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
)

$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot 'CMakeLists.txt'))) {
    $sourceRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'upstream\yacreader'
}
$sourceRoot = (Resolve-Path -LiteralPath $sourceRoot).Path
$projectRoot = $sourceRoot
$outerWorkspace = [IO.Path]::GetFullPath((Join-Path $sourceRoot '..\..'))
if (Test-Path -LiteralPath (Join-Path $outerWorkspace '.build-tools')) {
    $projectRoot = $outerWorkspace
}
$payloadRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
$artifactsRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts')) + '\'
if (!$payloadRoot.StartsWith($artifactsRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release staging must be inside the workspace artifacts directory.'
}
if (Test-Path -LiteralPath $payloadRoot) {
    throw "Use a fresh output directory so old files cannot enter a release: $payloadRoot"
}
$python = Join-Path $projectRoot '.build-tools\python\Scripts\python.exe'
$checker = Join-Path $sourceRoot 'scripts\local\check-reference-isolation.py'
$buildRoot = Join-Path $sourceRoot 'build-baseline'
& $python $checker --source $sourceRoot --build $buildRoot
if ($LASTEXITCODE -ne 0) { throw 'Reference isolation check failed.' }

# Deploy copies named applications and runtime dependencies, never the source tree
# or previous acceptance directories. Keep all three apps in this first pass.
& (Join-Path $sourceRoot 'scripts\local\build-baseline.ps1') -Step Deploy -OutputDirectory $OutputDirectory -ReleasePayload | Out-Null
& $python $checker --source $sourceRoot --build $buildRoot --payload $payloadRoot --manifest ($payloadRoot + '.manifest.json')
if ($LASTEXITCODE -ne 0) { throw 'Payload audit failed; do not distribute this directory.' }
Write-Host "Clean release payload: $payloadRoot"
Write-Host "SHA-256 manifest: $payloadRoot.manifest.json"
