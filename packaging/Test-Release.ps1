param([string]$ReleaseDirectory)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot
if (-not $ReleaseDirectory) {
    $taskCmake=[IO.File]::ReadAllText((Join-Path $taskRoot 'CMakeLists.txt'))
    if($taskCmake -notmatch 'project\(VBANPlug VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Version not found.' }
    $ReleaseDirectory=Join-Path $taskRoot ('out\releases\'+$Matches[1])
}
$taskSetup=@(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*-Setup.exe')
$taskZip=@(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*-Portable.zip')
if($taskSetup.Count -ne 1 -or $taskZip.Count -ne 1) { throw 'Expected one installer and one ZIP.' }
$taskRegistry='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{826C169A-BC2B-48D7-9442-C877455C006F}_is1'
if(Test-Path -LiteralPath $taskRegistry) { throw 'A previous current-user VBAN Plug installation exists. Test aborted without modifying it.' }
$taskTest=Join-Path $taskRoot ('build\package-tests\'+[guid]::NewGuid().ToString('N'))
$taskExtract=Join-Path $taskTest 'extracted'
$taskInstall=Join-Path $taskTest 'installed'
New-Item -ItemType Directory -Force -Path $taskExtract,$taskInstall | Out-Null
Expand-Archive -LiteralPath $taskZip[0].FullName -DestinationPath $taskExtract
$taskPayload=Join-Path $taskExtract 'VBAN Plug'
foreach($taskLine in [IO.File]::ReadAllLines((Join-Path $taskPayload 'SHA256.txt'))) {
    if($taskLine -notmatch '^([a-f0-9]{64})  (.+)$') { throw 'Malformed ZIP manifest.' }
    $taskExpected=$Matches[1]; $taskRelative=$Matches[2]
    $taskResolved=[IO.Path]::GetFullPath((Join-Path $taskPayload $taskRelative))
    if(-not $taskResolved.StartsWith($taskPayload+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escapes package.' }
    if((Get-FileHash -LiteralPath $taskResolved -Algorithm SHA256).Hash -ne $taskExpected) { throw "ZIP hash mismatch: $taskRelative" }
}
$taskSentinel=Join-Path $taskInstall 'VST3\Unrelated test plugin\keep.txt'
New-Item -ItemType Directory -Force -Path (Split-Path $taskSentinel) | Out-Null
[IO.File]::WriteAllText($taskSentinel,'Do not delete unrelated plug-ins.')
$taskUninstaller=Join-Path $taskInstall 'unins000.exe'
try {
    for($taskPass=1;$taskPass -le 2;$taskPass++) {
        $taskArgs=@('/CURRENTUSER','/SP-','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CLOSEAPPLICATIONS=no',('/DIR="'+$taskInstall+'"'),('/LOG="'+(Join-Path $taskTest "install-$taskPass.log")+'"'))
        $taskProcess=Start-Process -FilePath $taskSetup[0].FullName -ArgumentList $taskArgs -Wait -PassThru -WindowStyle Hidden
        if($taskProcess.ExitCode -ne 0) { throw "Installer exited with $($taskProcess.ExitCode); see $taskTest" }
        foreach($taskDirection in @('TX','RX')) {
            $taskBundle="VBAN Plug $taskDirection.vst3"
            foreach($taskFile in Get-ChildItem -LiteralPath (Join-Path $taskPayload $taskBundle) -File -Recurse) {
                $taskRelative=$taskFile.FullName.Substring($taskPayload.Length+1)
                $taskInstalled=Join-Path (Join-Path $taskInstall 'VST3') $taskRelative
                if((Get-FileHash -LiteralPath $taskFile.FullName).Hash -ne (Get-FileHash -LiteralPath $taskInstalled).Hash) { throw "Installed bundle mismatch: $taskRelative" }
            }
        }
    }
    $taskTester=Join-Path $taskRoot 'build\vs2026-insiders-x64\VBANPlugTests_artefacts\Release\VBANPlugTests.exe'
    & $taskTester (Join-Path $taskTest 'previews') (Join-Path $taskInstall 'VST3\VBAN Plug TX.vst3') (Join-Path $taskInstall 'VST3\VBAN Plug RX.vst3') *> (Join-Path $taskTest 'plugin-tests.txt')
    if($LASTEXITCODE) { throw "Installed plug-in tests failed; see $taskTest" }
} finally {
    if(Test-Path -LiteralPath $taskUninstaller) {
        # Only invoke the uninstaller inside the fresh workspace-owned test directory.
        $taskResolvedUninstaller=(Resolve-Path -LiteralPath $taskUninstaller).Path
        if(-not $taskResolvedUninstaller.StartsWith($taskTest+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected uninstaller path.' }
        $taskArgs=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $taskTest 'uninstall.log')+'"'))
        $taskProcess=Start-Process -FilePath $taskResolvedUninstaller -ArgumentList $taskArgs -Wait -PassThru -WindowStyle Hidden
        if($taskProcess.ExitCode -ne 0) { throw "Uninstaller exited with $($taskProcess.ExitCode)." }
    }
}
if(-not (Test-Path -LiteralPath $taskSentinel)) { throw 'Uninstaller removed an unrelated file.' }
foreach($taskDirection in @('TX','RX')) {
    if(Test-Path -LiteralPath (Join-Path $taskInstall "VST3\VBAN Plug $taskDirection.vst3")) { throw 'Plug-in remained after uninstall.' }
}
if(Test-Path -LiteralPath $taskRegistry) { throw 'Test uninstall registration remained.' }
$taskSummary='PASS: ZIP manifest, install, reinstall, installed payload hashes, audio/VST3 loading, uninstall, unrelated-file preservation.'
[IO.File]::WriteAllText((Join-Path $taskTest 'RESULT.txt'),$taskSummary)
Write-Host $taskSummary
Write-Host "Logs: $taskTest"
