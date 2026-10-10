param([string]$BuildDirectory = 'build', [string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $projectRoot $BuildDirectory
$testExecutable = Join-Path $buildPath "$Configuration/desktop_tests.exe"
$testLog = Join-Path $buildPath 'gpu-ink-tests.txt'
$environment = @{
    QT_QPA_PLATFORM = 'windows'
    QT_QUICK_BACKEND = 'rhi'
    QSG_RHI_BACKEND = 'd3d11'
    QSG_RHI_SHADER_CACHE_DIR = (Join-Path $buildPath 'gpu-shader-cache')
}
$previousEnvironment = @{}
foreach ($key in $environment.Keys) {
    $previousEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
}

Push-Location $projectRoot
try {
    & cmake --build $buildPath --config $Configuration --target desktop_tests --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Failed to build desktop_tests.' }
    foreach ($key in $environment.Keys) {
        [Environment]::SetEnvironmentVariable($key, $environment[$key], 'Process')
    }
    Push-Location $buildPath
    try {
        & $testExecutable manyMarkersRefineAsynchronouslyAndIgnoreStaleResults compactPenMeshPreservesPressureCurvesAndRoundEnds densePenZoomRetainsMeshesAndCullsInvisibleInk densePenOnlyZoomRendersOnGpu fastPenRemainsVisibleAfterRelease largePenMeshesKeepEveryTriangleAndPersist markerPreviewReusesUntouchedTiles fastMarkerErasingReusesRenderedTiles mixedEraserFastSweepsPreserveRecoveryAndUnrelatedObjects denseInkZoomReusesTexturesInBothDirections zoomReusesErasedShapeTexturesUntilSettled -o "$testLog,txt"
        $testExit = $LASTEXITCODE
        Get-Content -LiteralPath $testLog
        if ($testExit -ne 0) { throw "GPU rendering tests failed (exit $testExit)." }
    } finally { Pop-Location }
} finally {
    foreach ($key in $environment.Keys) {
        [Environment]::SetEnvironmentVariable($key, $previousEnvironment[$key], 'Process')
    }
    Pop-Location
}
