param([switch]$SmokeTest, [switch]$TestRefresh)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Microsoft C++ Build Tools with Desktop development with C++.' }
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vsPath) { throw 'MSVC x64/x86 tools were not found.' }
    $cmake = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if (!(Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    & $cmake -S . -B build -A x64 -DBUILD_TESTING=ON
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmake --build build --config Release --parallel
    if ($LASTEXITCODE) { throw 'Build failed.' }
    & $ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
    New-Item -ItemType Directory -Force dist | Out-Null
    Copy-Item -LiteralPath build/Release/fps-timer.exe -Destination dist/fps-timer.exe
    if ($SmokeTest) {
        $testArgs = @('--smoke-test')
        if ($TestRefresh) { $testArgs += '--test-refresh' }
        $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'dist/fps-timer.exe') -ArgumentList $testArgs -WindowStyle Hidden -PassThru -Wait
        if ($process.ExitCode -ne 0) { throw 'Graphics smoke test failed. See build/smoke-test.txt.' }
        Get-Content build/smoke-test.txt
    }
    Write-Host 'Built dist/fps-timer.exe (portable, Windows x64)'
} finally { Pop-Location }
