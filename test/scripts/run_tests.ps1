Set-StrictMode -Version Latest -Off
$testDir = Join-Path $PSScriptRoot '..'
$binDir = Join-Path $testDir 'build\\bin'

if (-Not (Test-Path $binDir)) {
    Write-Error "Tests not built. Run test/scripts/build_tests.ps1 first."
    exit 1
}

Write-Host "Running tests in $binDir"
$outputDir = Join-Path $testDir 'output'
if (-Not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}
$total = 0
$failures = @()
Get-ChildItem -Path $binDir -File | ForEach-Object {
    $exe = $_.FullName
    $name = $_.Name
    Write-Host "`n--- Running $name ---"
    & $exe
    $rc = $LASTEXITCODE
    $total++
    if ($rc -ne 0) {
        Write-Error "Test $name failed with exit code $rc"
        $failures += "$name (exit $rc)"
    }
}

Write-Host "`nRan $total test(s). Failures: $($failures.Count)"
if ($failures.Count -eq 0) {
    Write-Host "All tests passed"
    exit 0
} else {
    Write-Host "Failed tests:"
    $failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
