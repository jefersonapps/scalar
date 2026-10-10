# Build e execução dos testes do núcleo (Core) no Windows com Visual Studio (MSVC)
# Não requer CMake nem Qt.
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = (Resolve-Path "$scriptDir\..").Path
Set-Location $rootDir

# Garantir que o ambiente MSVC está disponível
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvars) {
                Write-Host "Inicializando ambiente MSVC x64 de $vcvars..." -ForegroundColor Cyan
                $envLines = cmd.exe /c "call `"$vcvars`" >nul && set"
                foreach ($line in $envLines) {
                    if ($line -match "^([^=]+)=(.*)$") {
                        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                    }
                }
            }
        }
    }
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Error "Compilador cl.exe não encontrado. Instale o Visual Studio com o workload C++ Desktop."
    exit 1
}

$buildDir = Join-Path $rootDir "build-core"
$coreObjDir = Join-Path $buildDir "obj-core"
$testObjDir = Join-Path $buildDir "obj-tests"

if (-not (Test-Path $coreObjDir)) { New-Item -ItemType Directory -Path $coreObjDir -Force | Out-Null }
if (-not (Test-Path $testObjDir)) { New-Item -ItemType Directory -Path $testObjDir -Force | Out-Null }

Write-Host "Compilando biblioteca estática scalar_core..." -ForegroundColor Cyan
$coreSources = @(
    "src\documents\Document.cpp",
    "src\commands\History.cpp",
    "src\rendering\StrokeMesh.cpp",
    "src\rendering\BackgroundMesh.cpp",
    "src\persistence\ZipArchive.cpp",
    "src\geometry\Geometry.cpp",
    "src\recognition\ShapeRecognizer.cpp",
    "src\rendering\ShapeMesh.cpp",
    "src\tools\StrokeEraser.cpp",
    "src\tools\GeometryTools.cpp"
)

& cl.exe /nologo /c /O2 /EHsc /std:c++20 /W4 /Isrc $coreSources /Fo:"$coreObjDir\"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$coreObjs = Get-ChildItem -Path $coreObjDir -Filter "*.obj" | ForEach-Object { $_.FullName }
& lib.exe /nologo /OUT:"$buildDir\scalar_core.lib" $coreObjs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Compilando testes do núcleo..." -ForegroundColor Cyan
$tests = @(
    @{ Name = "core_tests"; Source = "tests\core_tests.cpp"; Args = @("build-core\fixture.board") },
    @{ Name = "geometry_tools_tests"; Source = "tests\geometry_tools_tests.cpp"; Args = @() },
    @{ Name = "recognition_tests"; Source = "tests\recognition_tests.cpp"; Args = @() },
    @{ Name = "eraser_tests"; Source = "tests\eraser_tests.cpp"; Args = @() }
)

foreach ($test in $tests) {
    Write-Host "Compilando $($test.Name)..." -ForegroundColor Cyan
    & cl.exe /nologo /O2 /EHsc /std:c++20 /W4 /Isrc $test.Source "$buildDir\scalar_core.lib" /Fo:"$testObjDir\" /Fe:"$buildDir\$($test.Name).exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "`nExecutando testes do núcleo:" -ForegroundColor Green
$allPassed = $true
foreach ($test in $tests) {
    Write-Host "--- $($test.Name) ---" -ForegroundColor Yellow
    $exe = "$buildDir\$($test.Name).exe"
    & $exe $test.Args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FALHA em $($test.Name)" -ForegroundColor Red
        $allPassed = $false
    }
}

if ($allPassed) {
    Write-Host "`nTodos os testes do núcleo passaram com sucesso no MSVC!" -ForegroundColor Green
} else {
    Write-Host "`nHouve falha em um ou mais testes." -ForegroundColor Red
    exit 1
}
