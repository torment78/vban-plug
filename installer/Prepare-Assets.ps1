$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$taskImage = [Drawing.Image]::FromFile((Join-Path $PSScriptRoot 'Assets\VBAN-Plug.png'))
$taskSizes = @(16,24,32,48,64,128,256)
$taskFrames = @()
try {
    foreach ($taskSize in $taskSizes) {
        $taskBitmap = New-Object Drawing.Bitmap $taskSize,$taskSize
        $taskGraphics = [Drawing.Graphics]::FromImage($taskBitmap)
        $taskStream = New-Object IO.MemoryStream
        try {
            $taskGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $taskGraphics.DrawImage($taskImage,0,0,$taskSize,$taskSize)
            $taskBitmap.Save($taskStream,[Drawing.Imaging.ImageFormat]::Png)
            $taskFrames += ,$taskStream.ToArray()
        } finally { $taskStream.Dispose(); $taskGraphics.Dispose(); $taskBitmap.Dispose() }
    }
} finally { $taskImage.Dispose() }
$taskOutput = [IO.File]::Create((Join-Path $PSScriptRoot 'Assets\VBAN-Plug.ico'))
$taskWriter = New-Object IO.BinaryWriter $taskOutput
try {
    $taskWriter.Write([uint16]0); $taskWriter.Write([uint16]1); $taskWriter.Write([uint16]$taskSizes.Count)
    $taskOffset = 6 + 16 * $taskSizes.Count
    for ($taskIndex=0; $taskIndex -lt $taskSizes.Count; $taskIndex++) {
        $taskDimension = $taskSizes[$taskIndex] % 256
        $taskWriter.Write([byte]$taskDimension); $taskWriter.Write([byte]$taskDimension)
        $taskWriter.Write([uint16]0); $taskWriter.Write([uint16]1); $taskWriter.Write([uint16]32)
        $taskWriter.Write([uint32]$taskFrames[$taskIndex].Length); $taskWriter.Write([uint32]$taskOffset)
        $taskOffset += $taskFrames[$taskIndex].Length
    }
    foreach ($taskFrame in $taskFrames) { $taskWriter.Write([byte[]]$taskFrame) }
} finally { $taskWriter.Dispose(); $taskOutput.Dispose() }
