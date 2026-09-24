param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Open,
    [switch]$ConfigureOnly,
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
if ($Package -and ($ConfigureOnly -or $Configuration -ne 'Release')) { throw '-Package requires a full Release build.' }
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2026 Insiders with Desktop development with C++ and CMake tools.'
}
$taskInstances = @(& $taskVswhere -prerelease -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8 | ConvertFrom-Json)
if ($LASTEXITCODE) { throw 'Could not query installed Visual Studio instances.' }
$taskInstance = $taskInstances |
    Where-Object { $_.isPrerelease -and $_.isComplete -and $_.productPath -like '*\devenv.exe' } |
    Sort-Object -Property { [version]$_.installationVersion } -Descending |
    Select-Object -First 1
if (-not $taskInstance) {
    throw 'Visual Studio 2026 Insiders with Desktop development with C++ was not found.'
}
$taskVS = $taskInstance.installationPath
$taskIde = $taskInstance.productPath
$taskCmake = Join-Path $taskVS 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $taskCmake)) {
    throw 'Install C++ CMake tools for Windows in Visual Studio 2026 Insiders.'
}
$taskBuild = Join-Path $PSScriptRoot 'build/vs2026-insiders-x64'
Write-Host "Using Visual Studio 2026 Insiders $($taskInstance.installationVersion): $taskVS"
Push-Location $PSScriptRoot
try {
    & $taskCmake --preset vs2026-insiders-x64 "-DCMAKE_GENERATOR_INSTANCE:PATH=$taskVS"
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    if (-not $ConfigureOnly) {
        & $taskCmake --build $taskBuild --config $Configuration --parallel 4
        if ($LASTEXITCODE) { throw 'Build failed.' }
        $taskCtest = Join-Path (Split-Path $taskCmake) 'ctest.exe'
        & $taskCtest --test-dir $taskBuild -C $Configuration --output-on-failure
        if ($LASTEXITCODE) { throw 'Tests failed.' }
        Write-Host 'Both VST3 plug-ins and integration checks are ready.'
    }
    if ($Package) { & (Join-Path $PSScriptRoot 'Build-Release.ps1') -SkipBuild }
    if ($Open) {
        $taskSolution = Join-Path $taskBuild 'VBANPlug.slnx'
        if (-not (Test-Path -LiteralPath $taskSolution)) { $taskSolution = Join-Path $taskBuild 'VBANPlug.sln' }
        if (-not (Test-Path -LiteralPath $taskSolution)) { throw "Generated Insiders solution was not found: $taskSolution" }
        Start-Process -FilePath $taskIde -ArgumentList ('"' + $taskSolution + '"')
    }
} finally { Pop-Location }
