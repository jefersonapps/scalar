# Script de geração do instalador autônomo do Scalar para Windows (.exe)
# Prepara as dependências, compila com MSVC + Qt 6 e empacota via CPack + NSIS
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = (Resolve-Path "$scriptDir\..").Path
Set-Location $rootDir

# Garantir ferramentas no PATH
$cmakeBin = "C:\Program Files\CMake\bin"
$nsisBin = "C:\NSIS\nsis-3.10"
$pythonScripts = "$env:LOCALAPPDATA\Python\pythoncore-3.14-64\Scripts"
$qtBin = "C:\Qt\6.8.2\msvc2022_64\bin"

$extraPaths = @($cmakeBin, $nsisBin, $pythonScripts, $qtBin) | Where-Object { Test-Path $_ }
$env:PATH = ($extraPaths -join ";") + ";" + $env:PATH

# Inicializar ambiente MSVC se cl.exe não estiver carregado
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvars) {
                Write-Host "Carregando ambiente Visual Studio x64..." -ForegroundColor Cyan
                $cmdOutput = cmd.exe /c "call `"$vcvars`" >nul && set"
                foreach ($line in $cmdOutput) {
                    $parts = $line -split "=", 2
                    if ($parts.Length -eq 2) {
                        [System.Environment]::SetEnvironmentVariable($parts[0], $parts[1], "Process")
                    }
                }
            }
        }
    }
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Error "Compilador cl.exe não encontrado."
    exit 1
}

$qtPrefix = "C:/Qt/6.8.2/msvc2022_64"
if (-not (Test-Path $qtPrefix)) {
    Write-Error "Qt não encontrado em $qtPrefix."
    exit 1
}

Write-Host "Configurando o projeto com CMake e Ninja..." -ForegroundColor Cyan
Remove-Item -Path "build-package" -Recurse -Force -ErrorAction SilentlyContinue

& cmake -S . -B build-package -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$qtPrefix" `
    -DBUILD_TESTING=OFF `
    -DSCALAR_WEBP_INCLUDE_DIR=IGNORE `
    -DSCALAR_WEBP_LIBRARY=IGNORE

if ($LASTEXITCODE -ne 0) {
    Write-Error "Falha na configuração do CMake."
    exit $LASTEXITCODE
}

Write-Host "Compilando Scalar para Windows..." -ForegroundColor Cyan
& cmake --build build-package --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "Falha na compilação do Scalar."
    exit $LASTEXITCODE
}

Write-Host "Criando o instalador final executável (.exe) via CPack e NSIS..." -ForegroundColor Cyan
& cpack --config build-package/CPackConfig.cmake -G NSIS
if ($LASTEXITCODE -ne 0) {
    Write-Error "Falha ao gerar o instalador com NSIS."
    exit $LASTEXITCODE
}

$packages = Get-ChildItem -Path "build-package/packages" -Filter "*.exe" -ErrorAction SilentlyContinue
Write-Host "`nInstalador gerado com sucesso para distribuição!" -ForegroundColor Green
foreach ($pkg in $packages) {
    Write-Host "  -> $($pkg.FullName) ($([math]::round($pkg.Length/1MB, 2)) MB)" -ForegroundColor Yellow
}
