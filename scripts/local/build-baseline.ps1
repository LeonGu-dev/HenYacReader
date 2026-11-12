param(
    [ValidateSet('Configure', 'Build', 'Test', 'Deploy', 'All')]
    [string]$Step = 'All',
    [string[]]$Target = @(),
    [string]$OutputDirectory = 'artifacts\baseline',
    [switch]$ReleasePayload
)

$ErrorActionPreference = 'Stop'
# Support both a normal clone and the original workspace's compatibility junction.
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
$buildRoot = Join-Path $sourceRoot 'build-baseline'
$toolsRoot = Join-Path $projectRoot '.build-tools'
$qtRoot = Join-Path $toolsRoot 'Qt\6.9.3\msvc2022_64'
$vcvars = Join-Path $toolsRoot 'VS2022\VC\Auxiliary\Build\vcvars64.bat'
$toolBin = Join-Path $toolsRoot 'python\Scripts'
$logRoot = Join-Path $projectRoot 'artifacts\logs'
$deployRoot = Join-Path $projectRoot $OutputDirectory

if (!(Test-Path -LiteralPath $vcvars)) { throw "Missing MSVC environment: $vcvars" }
if (!(Test-Path -LiteralPath "$qtRoot\bin\qmake.exe")) { throw "Missing Qt: $qtRoot" }
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

# Import the compiler environment into this process; no persistent PATH changes.
$compilerEnvironment = & $env:ComSpec /d /s /c "`"`"$vcvars`" >nul && set`""
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC.' }
foreach ($entry in $compilerEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
$env:PATH = "$toolBin;$qtRoot\bin;$sourceRoot\dependencies\pdfium\win\x64;$env:PATH"
$env:VSLANG = '1033'

function Invoke-Logged {
    param([string]$Executable, [string[]]$Arguments, [string]$Log)
    & $Executable @Arguments 2>&1 |
        Tee-Object -FilePath (Join-Path $logRoot $Log) |
        Where-Object { $_ -notmatch '^(注意: 包含文件:|Note: including file:)' -and $_ -notmatch '^[^\[]+:\s{2,}[A-Za-z]:\\' }
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE. See $Log." }
}

if ($Step -in 'Configure', 'All') {
    Invoke-Logged "$toolBin\cmake.exe" @(
        '--fresh', '-S', $sourceRoot, '-B', $buildRoot, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_PREFIX_PATH=$qtRoot", '-DCMAKE_CXX_SCAN_FOR_MODULES=OFF',
        '-DDECOMPRESSION_BACKEND=7zip', '-DPDF_BACKEND=pdfium', '-DBUILD_TESTS=ON'
    ) 'configure.log'
    # A fresh configuration may change MSVC's dependency-output language.
    # Rescan all object dependencies on the next build rather than reuse stale ones.
    $dependencyLog = Join-Path $buildRoot '.ninja_deps'
    if (Test-Path -LiteralPath $dependencyLog) {
        Remove-Item -LiteralPath $dependencyLog
    }
}
if ($Step -in 'Build', 'All') {
    & "$toolBin\python.exe" (Join-Path $PSScriptRoot 'repair-msvc-dependencies.py') $buildRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not repair MSVC dependency tracking.' }
    $buildArguments = @(
        '--build', $buildRoot, '--parallel', [string][Environment]::ProcessorCount
    )
    if ($Target.Count -gt 0) { $buildArguments += @('--target') + $Target }
    Invoke-Logged "$toolBin\cmake.exe" $buildArguments 'build.log'
    if ($Target.Count -eq 0 -or $Target -contains 'YACReaderLibrary') {
        $dependencies = & "$toolBin\ninja.exe" -C $buildRoot -t deps 'YACReaderLibrary/CMakeFiles/YACReaderLibrary.dir/properties_dialog.cpp.obj'
        if ($LASTEXITCODE -ne 0 -or !($dependencies -match 'comic_metadata_editor\.h')) {
            throw 'MSVC header dependency tracking is incomplete; refusing this build.'
        }
        Write-Host 'Verified metadata editor header dependencies.'
    }
}
if ($Step -in 'Test', 'All') {
    $testTemp = Join-Path $projectRoot 'artifacts\test-temp'
    New-Item -ItemType Directory -Force -Path $testTemp | Out-Null
    $env:TEMP = $testTemp
    $env:TMP = $testTemp
    Invoke-Logged "$toolBin\ctest.exe" @(
        '--test-dir', $buildRoot, '--output-on-failure'
    ) 'test.log'
}
if ($Step -in 'Deploy', 'All') {
    New-Item -ItemType Directory -Force -Path $deployRoot | Out-Null
    foreach ($appName in 'YACReader', 'YACReaderLibrary', 'YACReaderLibraryServer') {
        Copy-Item -LiteralPath "$buildRoot\bin\$appName.exe" -Destination $deployRoot -Force
    }
    $deployArguments = @(
        '--release', '--compiler-runtime', '--qmldir', "$sourceRoot\YACReaderLibrary\qml",
        "$deployRoot\YACReader.exe", "$deployRoot\YACReaderLibrary.exe", "$deployRoot\YACReaderLibraryServer.exe"
    )
    if ($ReleasePayload) { $deployArguments += @('--skip-plugin-types', 'qmltooling') }
    Invoke-Logged "$qtRoot\bin\windeployqt.exe" $deployArguments 'deploy.log'
    New-Item -ItemType Directory -Force -Path "$deployRoot\utils", "$deployRoot\openssl", "$deployRoot\languages" | Out-Null
    Copy-Item -LiteralPath "$sourceRoot\dependencies\7zip\win\x64\7z.dll" -Destination "$deployRoot\utils" -Force
    Copy-Item -LiteralPath "$sourceRoot\dependencies\pdfium\win\x64\pdfium.dll" -Destination $deployRoot -Force
    Copy-Item -Path "$sourceRoot\dependencies\openssl\win\x64\*" -Destination "$deployRoot\openssl" -Force
    Copy-Item -LiteralPath "$sourceRoot\COPYING.txt", "$sourceRoot\README.md" -Destination $deployRoot -Force
    if ($ReleasePayload) {
        Copy-Item -LiteralPath "$sourceRoot\release\portable-README.md" -Destination "$deployRoot\README.md" -Force
        New-Item -ItemType Directory -Path "$deployRoot\licenses" -Force | Out-Null
        Copy-Item -LiteralPath "$sourceRoot\third_party\QsLog\LICENSE.txt" -Destination "$deployRoot\licenses\QsLog-LICENSE.txt" -Force
        Copy-Item -LiteralPath "$sourceRoot\release\THIRD-PARTY-NOTICES.md" -Destination "$deployRoot\licenses" -Force
    }
    Copy-Item -LiteralPath "$sourceRoot\release\server" -Destination $deployRoot -Recurse -Force
    Get-ChildItem -LiteralPath $buildRoot -Recurse -Filter '*.qm' |
        Where-Object { $_.BaseName -notlike '*_source' } |
        Copy-Item -Destination "$deployRoot\languages" -Force
    Write-Host "Baseline apps: $deployRoot"
}
