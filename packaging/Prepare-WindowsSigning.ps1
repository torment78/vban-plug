# Copyright (c) 2026 ElkaSoft
# SPDX-License-Identifier: AGPL-3.0-only
param([string]$InnoCompiler)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if ([IO.File]::ReadAllText((Join-Path $taskRoot 'CMakeLists.txt')) -notmatch 'project\(VBANPlug VERSION ([\d.]+)') { throw 'Version not found' }
$taskVersion=$Matches[1]
$taskState=Get-Content -LiteralPath (Join-Path $taskRoot "out\releases\$taskVersion\packaging-state.json") -Raw | ConvertFrom-Json
if ($taskState.version -ne $taskVersion -or -not (Test-Path -LiteralPath $taskState.payload)) { throw 'Run Build-Release.ps1 -SkipBuild first.' }
if (-not $InnoCompiler) { $InnoCompiler=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Inno Setup 6\ISCC.exe' }
$taskSigning=Join-Path $taskRoot "out\signing\$taskVersion"
$taskUninstaller=Join-Path $taskSigning 'uninstaller'
New-Item -ItemType Directory -Force -Path $taskUninstaller | Out-Null
$taskStart=[Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName=$InnoCompiler
$taskStart.UseShellExecute=$false
$taskStart.CreateNoWindow=$true
$taskStart.RedirectStandardInput=$true
$taskStart.RedirectStandardOutput=$true
$taskStart.RedirectStandardError=$true
foreach ($taskArgument in @("/DAppVersion=$taskVersion", "/DPayloadDir=$($taskState.payload)", "/DOutputPath=$taskSigning", "/DSignedUninstallerOutputDir=$taskUninstaller", (Join-Path $taskRoot 'installer\VBAN-Plug.iss'))) { $taskStart.ArgumentList.Add($taskArgument) }
$taskProcess=[Diagnostics.Process]::new()
$taskProcess.StartInfo=$taskStart
try {
    if (-not $taskProcess.Start()) { throw 'Could not prepare the uninstaller' }
    $taskOutput=$taskProcess.StandardOutput.ReadToEndAsync()
    $taskError=$taskProcess.StandardError.ReadToEndAsync()
    $taskProcess.StandardInput.Close()
    if (-not $taskProcess.WaitForExit(30000)) { $taskProcess.Kill($true); throw 'Uninstaller preparation timed out' }
    $taskLog=$taskOutput.GetAwaiter().GetResult()+$taskError.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $taskSigning 'prepare-uninstaller.log'),$taskLog)
    # Inno intentionally stops at the first compile until this exact EXE is signed.
    if ($taskProcess.ExitCode -ne 0 -and $taskLog -notmatch 'Signed uninstaller mode is enabled') { throw 'Installer preparation failed; read prepare-uninstaller.log' }
    $taskFiles=@(Get-ChildItem -LiteralPath $taskUninstaller -Filter '*.e32' -File)
    if ($taskFiles.Count -ne 1) { throw 'Expected exactly one uninstaller to sign' }
    Write-Output ('Prepared uninstaller: '+$taskFiles[0].FullName)
} finally { $taskProcess.Dispose() }
