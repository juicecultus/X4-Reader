Param(
    [switch]$Release
)

Set-StrictMode -Version Latest
$testDir = Join-Path $PSScriptRoot '..'
$rootDir = Join-Path $testDir '..'
$buildDir = Join-Path $testDir 'build'
New-Item -Force -ItemType Directory -Path $buildDir | Out-Null

$cmakeType = if ($Release) { 'Release' } else { 'Debug' }
cmake -S "$rootDir" -B "$buildDir" -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=$cmakeType
cmake --build "$buildDir" --config $cmakeType -- /m

Write-Host "Built tests. Executables are in $buildDir\bin"
