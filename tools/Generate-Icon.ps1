param(
    [string]$Source = (Join-Path $PSScriptRoot '../src/assets/icon-source.png'),
    [string]$Output = (Join-Path $PSScriptRoot '../src/assets/app.ico')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$iconSizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$frames = [System.Collections.Generic.List[byte[]]]::new()
$sourceBitmap = [Drawing.Bitmap]::new([IO.Path]::GetFullPath($Source))
try {
    foreach ($size in $iconSizes) {
        $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([Drawing.Color]::Transparent)
                $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                # Preserve the supplied artwork and alpha, fitting it to a square
                # with a small transparent margin at every Windows icon size.
                $scale = ($size * 0.94) / [Math]::Max($sourceBitmap.Width, $sourceBitmap.Height)
                $width = [single]($sourceBitmap.Width * $scale)
                $height = [single]($sourceBitmap.Height * $scale)
                $rect = [Drawing.RectangleF]::new([single](($size - $width) / 2), [single](($size - $height) / 2), $width, $height)
                $graphics.DrawImage($sourceBitmap, $rect)
            } finally { $graphics.Dispose() }
            $stream = [IO.MemoryStream]::new()
            try {
                $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
                $frames.Add($stream.ToArray())
            } finally { $stream.Dispose() }
        } finally { $bitmap.Dispose() }
    }
} finally { $sourceBitmap.Dispose() }

$file = [IO.File]::Create([IO.Path]::GetFullPath($Output))
$writer = [IO.BinaryWriter]::new($file)
try {
    $writer.Write([uint16]0) # Reserved
    $writer.Write([uint16]1) # ICO
    $writer.Write([uint16]$iconSizes.Count)
    $offset = 6 + 16 * $iconSizes.Count
    for ($index = 0; $index -lt $iconSizes.Count; $index++) {
        $dimension = if ($iconSizes[$index] -eq 256) { 0 } else { $iconSizes[$index] }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$index].Length)
        $writer.Write([uint32]$offset)
        $offset += $frames[$index].Length
    }
    foreach ($frame in $frames) { $writer.Write([byte[]]$frame) }
} finally { $writer.Dispose(); $file.Dispose() }
Write-Host "Generated $Output with sizes: $($iconSizes -join ', ')"
