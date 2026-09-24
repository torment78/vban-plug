param(
    [string]$InnoCompiler,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (-not $InnoCompiler) {
    $taskCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($taskCommand) { $InnoCompiler = $taskCommand.Source }
    else { $InnoCompiler = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Inno Setup 6\ISCC.exe' }
}
if (-not (Test-Path -LiteralPath $InnoCompiler)) { throw 'Install Inno Setup 6.6 or later, or pass -InnoCompiler with the path to ISCC.exe.' }
if (-not $SkipBuild) { & (Join-Path $taskRoot 'Build.ps1') -Configuration Release }
$taskCmakeText = [IO.File]::ReadAllText((Join-Path $taskRoot 'CMakeLists.txt'))
if ($taskCmakeText -notmatch 'project\(VBANPlug VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Could not read the project version.' }
$taskVersion = $Matches[1]
$taskBuild = Join-Path $taskRoot 'build\vs2026-insiders-x64'
$taskStage = Join-Path $taskRoot ('out\staging\' + [guid]::NewGuid().ToString('N'))
$taskPayload = Join-Path $taskStage 'VBAN Plug'
$taskOutput = Join-Path $taskRoot ('out\releases\' + $taskVersion)
New-Item -ItemType Directory -Force -Path $taskPayload,$taskOutput | Out-Null
foreach ($taskDirection in @('TX','RX')) {
    $taskBundleName = "VBAN Plug $taskDirection.vst3"
    $taskBundle = Join-Path $taskBuild "VBANPlug$($taskDirection)_artefacts\Release\VST3\$taskBundleName"
    foreach ($taskRelative in @("Contents\x86_64-win\$taskBundleName",'Contents\Resources\moduleinfo.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $taskBundle $taskRelative))) { throw "Incomplete plug-in bundle: $taskBundle\$taskRelative" }
    }
    Copy-Item -LiteralPath $taskBundle -Destination $taskPayload -Recurse
}
foreach ($taskDocument in @('README.md','LICENSE','NOTICE.md','CHANGELOG.md')) { Copy-Item -LiteralPath (Join-Path $taskRoot $taskDocument) -Destination $taskPayload }
if (Test-Path -LiteralPath (Join-Path $taskRoot 'docs\images')) {
    New-Item -ItemType Directory -Path (Join-Path $taskPayload 'docs') | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'docs\images') -Destination (Join-Path $taskPayload 'docs') -Recurse
    foreach ($taskDoc in @('RELEASING.md','INSTALL.md')) { Copy-Item -LiteralPath (Join-Path $taskRoot ('docs\'+ $taskDoc)) -Destination (Join-Path $taskPayload 'docs') }
}
$taskGuide = [IO.File]::ReadAllText((Join-Path $taskRoot 'packaging\START HERE.txt')).Replace('@VERSION@',$taskVersion)
[IO.File]::WriteAllText((Join-Path $taskPayload 'START HERE.txt'),$taskGuide)
# Use the exact JUCE checkout from the configured build.
$taskCache = [IO.File]::ReadAllLines((Join-Path $taskBuild 'CMakeCache.txt'))
$taskJuceLine = $taskCache | Where-Object { $_ -match '^JUCE_SOURCE_DIR:STATIC=' } | Select-Object -First 1
if (-not $taskJuceLine) { throw 'JUCE_SOURCE_DIR is missing from CMakeCache.txt; configure/build first.' }
$taskJuce = $taskJuceLine.Substring($taskJuceLine.IndexOf('=') + 1)
$taskNotices = Join-Path $taskPayload 'Third-party notices'
New-Item -ItemType Directory -Force -Path $taskNotices | Out-Null
$taskLicenses = @{
    'JUCE-LICENSE.md' = 'LICENSE.md'
    'VST3-SDK-LICENSE.txt' = 'modules\juce_audio_processors_headless\format_types\VST3_SDK\LICENSE.txt'
    'VST3-interfaces-LICENSE.txt' = 'modules\juce_audio_processors_headless\format_types\VST3_SDK\pluginterfaces\LICENSE.txt'
    'VST3-base-LICENSE.txt' = 'modules\juce_audio_processors_headless\format_types\VST3_SDK\base\LICENSE.txt'
    'VST3-public-SDK-LICENSE.txt' = 'modules\juce_audio_processors_headless\format_types\VST3_SDK\public.sdk\LICENSE.txt'
    'PNG-LICENSE.txt' = 'modules\juce_graphics\image_formats\pnglib\LICENSE'
    'zlib-LICENSE.txt' = 'modules\juce_core\zip\zlib\LICENSE'
    'SheenBidi-LICENSE.txt' = 'modules\juce_graphics\unicode\sheenbidi\LICENSE'
    'JPEG-README.txt' = 'modules\juce_graphics\image_formats\jpglib\README'
    'HarfBuzz-COPYING.txt' = 'modules\juce_graphics\fonts\harfbuzz\COPYING'
    'FLAC-LICENSE.txt' = 'modules\juce_audio_formats\codecs\flac\Flac Licence.txt'
    'Ogg-Vorbis-LICENSE.txt' = 'modules\juce_audio_formats\codecs\oggvorbis\Ogg Vorbis Licence.txt'
}
foreach ($taskEntry in $taskLicenses.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $taskJuce $taskEntry.Value) -Destination (Join-Path $taskNotices $taskEntry.Key)
}
$taskPayloadHashes = Get-ChildItem -LiteralPath $taskPayload -File -Recurse | Sort-Object FullName | ForEach-Object {
    $taskHash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $taskRelative = $_.FullName.Substring($taskPayload.Length+1).Replace('\','/')
    "$taskHash  $taskRelative"
}
[IO.File]::WriteAllLines((Join-Path $taskPayload 'SHA256.txt'),[string[]]$taskPayloadHashes)
& (Join-Path $taskRoot 'installer\Prepare-Assets.ps1')
& $InnoCompiler "/DAppVersion=$taskVersion" "/DPayloadDir=$taskPayload" "/DOutputPath=$taskOutput" (Join-Path $taskRoot 'installer\VBAN-Plug.iss')
if ($LASTEXITCODE) { throw 'Installer compilation failed.' }
$taskZip = Join-Path $taskOutput "VBAN-Plug-$taskVersion-Windows-x64-Portable.zip"
Compress-Archive -LiteralPath $taskPayload -DestinationPath $taskZip -CompressionLevel Optimal -Force
$taskSetup = Join-Path $taskOutput "VBAN-Plug-$taskVersion-Windows-x64-Setup.exe"
& (Join-Path $taskRoot 'packaging\New-SourceArchive.ps1') -JucePath $taskJuce -Version $taskVersion -OutputDirectory $taskOutput
$taskSourceZip = Join-Path $taskOutput "VBAN-Plug-$taskVersion-Source.zip"
$taskReleaseHashes = @($taskSetup,$taskZip,$taskSourceZip) | ForEach-Object {
    $taskHash = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    "$taskHash  $([IO.Path]::GetFileName($_))"
}
[IO.File]::WriteAllLines((Join-Path $taskOutput 'SHA256.txt'),[string[]]$taskReleaseHashes)
[IO.File]::WriteAllText((Join-Path $taskOutput 'START HERE.txt'),$taskGuide)
Write-Host "Release $taskVersion is ready: $taskOutput"
Write-Host "Installer: $taskSetup"
Write-Host "Portable:  $taskZip"
