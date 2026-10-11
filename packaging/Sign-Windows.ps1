# Copyright (c) 2026 ElkaSoft
# SPDX-License-Identifier: AGPL-3.0-only
param(
    [switch]$ResumeInstaller,
    [string]$CodeSignToolRoot = 'C:\tmp\CodeSignTool-v1.3.3-windows',
    [string]$ExpectedThumbprint = '17D34DB7C7A4AFA8FB9B2DEDD4C8CB208DC0014A',
    [string]$SignToolPath = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe',
    [string]$Python = 'python.exe'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if ([IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt')) -notmatch 'project\(VBANPlug VERSION ([\d.]+)') { throw 'Version not found' }
$version = $Matches[1]
$root = Join-Path $repo ('out\signing\' + $version)
New-Item -ItemType Directory -Force -Path $root | Out-Null
$log = Join-Path $root 'signing-progress.log'
$signTool = $SignToolPath
$toolRoot = $CodeSignToolRoot
$thumbprint = $ExpectedThumbprint
$credentialFile = Join-Path $env:LOCALAPPDATA 'ElkaSoft\Signing\esigner-credential-id.txt'
$records = [Collections.Generic.List[object]]::new()
function Stage([string]$message) {
    $line = '{0:HH:mm:ss} {1}' -f (Get-Date),$message
    Add-Content -LiteralPath $log -Value $line
    Write-Host $line
}
function Read-SigningInput([bool]$Account, [string]$Message) {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $form = [Windows.Forms.Form]::new()
    $form.Text = 'VBAN Plug - SSL.com signing'
    $form.StartPosition = 'CenterScreen'
    $form.ClientSize = [Drawing.Size]::new(510,220)
    $form.FormBorderStyle = 'FixedDialog'
    $form.MaximizeBox = $false
    $form.MinimizeBox = $false
    $form.TopMost = $true
    $label = [Windows.Forms.Label]::new()
    $label.Text = $Message
    $label.Location = [Drawing.Point]::new(20,15)
    $label.Size = [Drawing.Size]::new(470,55)
    $form.Controls.Add($label)
    $userBox = [Windows.Forms.TextBox]::new()
    $userBox.Location = [Drawing.Point]::new(20,80)
    $userBox.Size = [Drawing.Size]::new(470,25)
    $userBox.PlaceholderText = 'SSL.com account username'
    $userBox.Visible = $Account
    $form.Controls.Add($userBox)
    $secretBox = [Windows.Forms.TextBox]::new()
    $secretBox.Location = [Drawing.Point]::new(20,120)
    $secretBox.Size = [Drawing.Size]::new(470,25)
    $secretBox.UseSystemPasswordChar = $true
    $secretBox.PlaceholderText = if ($Account) { 'SSL.com password' } else { 'Six-digit phone code' }
    if (-not $Account) { $secretBox.MaxLength = 6 }
    $form.Controls.Add($secretBox)
    $ok = [Windows.Forms.Button]::new()
    $ok.Text = 'Continue'; $ok.DialogResult = 'OK'; $ok.Location = [Drawing.Point]::new(305,175)
    $cancel = [Windows.Forms.Button]::new()
    $cancel.Text = 'Cancel'; $cancel.DialogResult = 'Cancel'; $cancel.Location = [Drawing.Point]::new(410,175)
    $form.Controls.AddRange(@($ok,$cancel)); $form.AcceptButton=$ok; $form.CancelButton=$cancel
    $form.Add_Shown({ if ($Account) { $userBox.Focus() } else { $secretBox.Focus() } })
    try {
        if ($form.ShowDialog() -ne 'OK') { throw 'Signing cancelled by user.' }
        if ($Account) {
            if ([string]::IsNullOrWhiteSpace($userBox.Text) -or -not $secretBox.Text) { throw 'Signing account and password are required.' }
            return [pscredential]::new($userBox.Text.Trim(), (ConvertTo-SecureString $secretBox.Text -AsPlainText -Force))
        }
        if ($secretBox.Text -notmatch '^\d{6}$') { throw 'A six-digit code is required.' }
        return $secretBox.Text
    } finally { $secretBox.Clear(); $userBox.Clear(); $form.Dispose() }
}
function Invoke-CodeSign([string[]]$Arguments, [string]$Otp = '') {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $toolRoot 'jdk-11.0.2\bin\java.exe'
    $start.WorkingDirectory = $toolRoot
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true; $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    foreach ($value in @('-jar',(Join-Path $toolRoot 'jar\code_sign_tool-1.3.3.jar')) + $Arguments + @("-username=$($script:account.UserName)","-password=$script:password","-credential_id=$script:credentialId",'-program_name=VBAN Plug')) { $start.ArgumentList.Add($value) }
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'Could not start SSL.com CodeSignTool.' }
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if ($Otp) { $process.StandardInput.WriteLine($Otp) }
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(300000)) { $process.Kill($true); throw 'SSL.com operation timed out.' }
        $response = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        foreach ($secret in @($script:password,$script:account.UserName,$script:credentialId,$Otp)) {
            if ($secret) { $response = $response.Replace($secret,'[redacted]') }
        }
        if ($process.ExitCode -ne 0) { Stage $response; throw "SSL.com operation failed ($($process.ExitCode))." }
        Stage 'SSL.com operation completed successfully.'
    } finally { $start.ArgumentList.Clear(); $process.Dispose(); $response=$null; $Otp=$null }
}
function Verify([string]$file) {
    $sig = Get-AuthenticodeSignature -LiteralPath $file
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $thumbprint -or $null -eq $sig.TimeStamperCertificate) { throw "Signature/publisher/timestamp verification failed: $file" }
    & $signTool verify /pa /all $file >> $log 2>&1
    if ($LASTEXITCODE -ne 0) { throw "SignTool rejected $file" }
    $records.Add([pscustomobject]@{file=$file;status='Valid';publisher=$sig.SignerCertificate.Subject;thumbprint=$sig.SignerCertificate.Thumbprint;timestampAuthority=$sig.TimeStamperCertificate.Subject;sha256=(Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant()})
    Stage "Verified signature and timestamp: $([IO.Path]::GetFileName($file))"
}
function Sign-Batch([string]$Name, [object[]]$Mappings) {
    $batchRoot = Join-Path $root ($Name + '-' + [guid]::NewGuid().ToString('N'))
    $inputDir=Join-Path $batchRoot 'input'; $outputDir=Join-Path $batchRoot 'output'
    New-Item -ItemType Directory -Path $inputDir,$outputDir | Out-Null
    foreach ($mapping in $Mappings) {
        $inputFile=Join-Path $inputDir $mapping.Name
        Copy-Item -LiteralPath $mapping.Path -Destination $inputFile
        Stage "Malware scan: $($mapping.Name)"
        Invoke-CodeSign @('scan_code',"-input_file_path=$inputFile")
    }
    Stage "Waiting for phone code: $Name ($($Mappings.Count) files)."
    $otp=Read-SigningInput $false "Approve $Name ($($Mappings.Count) files). Enter a fresh SSL.com phone code. Use a new code if the previous one was already used."
    try { Invoke-CodeSign @('batch_sign',"-input_dir_path=$inputDir","-output_dir_path=$outputDir") $otp }
    finally { $otp=$null }
    foreach ($mapping in $Mappings) {
        $signed=Join-Path $outputDir $mapping.Name
        Verify $signed
        & $Python (Join-Path $PSScriptRoot 'verify-pe.py') (Join-Path $inputDir $mapping.Name) $signed
        if ($LASTEXITCODE -ne 0) { throw 'Signed payload changed beyond the signature fields.' }
        Copy-Item -LiteralPath $signed -Destination $mapping.Path -Force
    }
}

try {
    $certificate=Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object Thumbprint -eq $thumbprint
    if (-not $certificate -or $certificate.NotAfter -lt (Get-Date)) { throw 'Expected signing certificate is unavailable or expired.' }
    $script:credentialId=(Get-Content -LiteralPath $credentialFile -Raw).Trim()
    $build=Join-Path $repo 'build\vs2026-insiders-x64'
    $tx=Join-Path $build 'VBANPlugTX_artefacts\Release\VST3\VBAN Plug TX.vst3\Contents\x86_64-win\VBAN Plug TX.vst3'
    $rx=Join-Path $build 'VBANPlugRX_artefacts\Release\VST3\VBAN Plug RX.vst3\Contents\x86_64-win\VBAN Plug RX.vst3'
    $uninstallers=@(Get-ChildItem -LiteralPath (Join-Path $root 'uninstaller') -File -Filter '*.e32')
    if ($uninstallers.Count -ne 1) { throw 'Expected exactly one prepared uninstaller. See docs/RELEASING.md.' }
    $uninstaller=$uninstallers[0].FullName
    Stage 'Waiting for local SSL.com account entry; credentials are not saved.'
    $script:account=Read-SigningInput $true 'Enter your SSL.com account username and password. These stay in memory for this signing run and are never written to the repository or progress log.'
    $script:password=$script:account.GetNetworkCredential().Password
    if (-not $ResumeInstaller) {
        # Use .dll staging names for CodeSignTool; restore VST3 filenames after verification.
        Sign-Batch 'TX, RX and uninstaller' @(
            [pscustomobject]@{Path=$tx;Name='VBAN-Plug-TX.dll'},
            [pscustomobject]@{Path=$rx;Name='VBAN-Plug-RX.dll'},
            [pscustomobject]@{Path=$uninstaller;Name='VBAN-Plug-uninstaller.exe'})
    }
    Verify $tx
    Verify $rx
    Stage 'Packaging the signed plug-ins and uninstaller; no recompilation.'
    & (Join-Path $repo 'Build-Release.ps1') -SkipBuild -SignedUninstallerDirectory (Join-Path $root 'uninstaller') *> (Join-Path $root 'installer-build.log')
    if (-not $?) { throw 'Signed-payload packaging failed.' }
    $release=Join-Path $repo ('out\releases\' + $version)
    $setup=Join-Path $release "VBAN-Plug-$version-Windows-x64-Setup.exe"
    Sign-Batch 'final setup' @([pscustomobject]@{Path=$setup;Name=[IO.Path]::GetFileName($setup)})
    Verify $setup
    $records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'signature-verification.json') -Encoding utf8
    $hashes=@($setup,(Join-Path $release "VBAN-Plug-$version-Windows-x64-Portable.zip"),(Join-Path $release "VBAN-Plug-$version-Source.zip")) | ForEach-Object {
        (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_)
    }
    [IO.File]::WriteAllLines((Join-Path $release 'SHA256.txt'),[string[]]$hashes)
    Stage 'SIGNING COMPLETE. Testing installed signatures, audio, and uninstall.'
    & (Join-Path $PSScriptRoot 'Test-Release.ps1') -ReleaseDirectory $release -ExpectedSignerThumbprint $thumbprint *> (Join-Path $root 'installer-tests.log')
    if (-not $?) { throw 'Signed installer validation failed.' }
    Stage 'VERIFIED: signed Windows release is ready for publishing.'
} catch {
    Stage ('Signing stopped: ' + $_.Exception.Message)
    exit 1
} finally {
    $script:password=$null; $script:account=$null; $script:credentialId=$null
}
