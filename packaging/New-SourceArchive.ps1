param(
    [Parameter(Mandatory=$true)][string]$JucePath,
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot
$taskStage=Join-Path $taskRoot ('out\source-staging\'+[guid]::NewGuid().ToString('N'))
$taskSource=Join-Path $taskStage 'VBAN Plug'
New-Item -ItemType Directory -Force -Path $taskSource | Out-Null
Push-Location $taskRoot
try {
    $taskFiles=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard | Sort-Object -Unique)
    if($LASTEXITCODE) { throw 'Could not list source files.' }
    if(-not ($taskFiles -contains 'CMakeLists.txt') -or -not ($taskFiles -contains 'LICENSE')) { throw 'Incomplete source file list.' }
    foreach($taskRelative in $taskFiles) {
        $taskInput=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskRelative))
        $taskOutput=[IO.Path]::GetFullPath((Join-Path $taskSource $taskRelative))
        if(-not $taskInput.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
           -not $taskOutput.StartsWith($taskSource+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Source path escapes the project.' }
        New-Item -ItemType Directory -Force -Path (Split-Path $taskOutput) | Out-Null
        Copy-Item -LiteralPath $taskInput -Destination $taskOutput
    }
} finally { Pop-Location }
New-Item -ItemType Directory -Force -Path (Join-Path $taskSource 'external\JUCE') | Out-Null
foreach($taskItem in Get-ChildItem -LiteralPath $JucePath -Force) {
    if($taskItem.Name -ne '.git') { Copy-Item -LiteralPath $taskItem.FullName -Destination (Join-Path $taskSource 'external\JUCE') -Recurse }
}
$taskSourceInfo=@'
This archive contains VBAN Plug source and the JUCE source tree used by the build.
Open a PowerShell terminal in the VBAN Plug folder and run .\Build.ps1.
Visual Studio 2026 Insiders with Desktop development with C++ is required.
The external/JUCE directory allows configuration without downloading JUCE.

To reproduce the installer and portable ZIP from this source archive, initialize
a local Git working directory once (git init), then run .\Build.ps1 -Package.
Inno Setup 6.6 or later is required for packaging.

Project: https://github.com/torment78/vban-plug
Licence: GNU AGPL v3; see LICENSE and NOTICE.md.
Third-party licences remain in external/JUCE.
'@
[IO.File]::WriteAllText((Join-Path $taskSource 'BUILD FROM SOURCE.txt'),$taskSourceInfo)
$taskArchive=Join-Path $OutputDirectory "VBAN-Plug-$Version-Source.zip"
Compress-Archive -LiteralPath $taskSource -DestinationPath $taskArchive -CompressionLevel Optimal -Force
Write-Host "Source:   $taskArchive"
