Add-Type -AssemblyName System.Drawing

$proj = Split-Path -Parent $PSScriptRoot
$sheetPath = Join-Path $proj "pickups\#2 - Transparent Icons & Drop Shadow.png"
$sheet = [System.Drawing.Image]::FromFile($sheetPath)

$cell = 32
$col = 9   # column 10 (1-based)
$row = 5   # row 6 (1-based)
$src = New-Object System.Drawing.Rectangle ($col * $cell), ($row * $cell), $cell, $cell

$bmp32 = New-Object System.Drawing.Bitmap $cell, $cell
$g = [System.Drawing.Graphics]::FromImage($bmp32)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$dest = New-Object System.Drawing.Rectangle 0, 0, $cell, $cell
$g.DrawImage($sheet, $dest, $src, [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$sheet.Dispose()

$bmp32.Save((Join-Path $proj "game_icon.png"), [System.Drawing.Imaging.ImageFormat]::Png)

$bmp256 = New-Object System.Drawing.Bitmap 256, 256
$g2 = [System.Drawing.Graphics]::FromImage($bmp256)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g2.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g2.DrawImage($bmp32, 0, 0, 256, 256)
$g2.Dispose()

$icon = [System.Drawing.Icon]::FromHandle($bmp256.GetHicon())
$icoPath = Join-Path $proj "game.ico"
$fs = [System.IO.File]::Create($icoPath)
$icon.Save($fs)
$fs.Close()

Write-Host "Created game_icon.png and game.ico"
