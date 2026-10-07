# Draws the port's own app icon (docs/images/icon.png, used by the Linux
# AppImage): a green planet with a ring and an X on deep space blue. Original
# art, so it can ship in releases (the game's own icon cannot).
param([string]$Out = "$PSScriptRoot\..\docs\images\icon.png", [int]$Size = 256)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Size, $Size
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'AntiAliasGridFit'
$s = $Size / 256.0

# Rounded square background.
$r = 48 * $s
$bg = New-Object System.Drawing.Drawing2D.GraphicsPath
$bg.AddArc(0, 0, $r, $r, 180, 90); $bg.AddArc($Size - $r, 0, $r, $r, 270, 90)
$bg.AddArc($Size - $r, $Size - $r, $r, $r, 0, 90); $bg.AddArc(0, $Size - $r, $r, $r, 90, 90); $bg.CloseFigure()
$fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point 0, $Size), ([System.Drawing.Color]::FromArgb(255, 18, 28, 58)), ([System.Drawing.Color]::FromArgb(255, 6, 10, 24))
$g.FillPath($fill, $bg)

# Stars.
$rng = New-Object System.Random 7
for ($i = 0; $i -lt 40; $i++) {
    $d = (1 + $rng.Next(3)) * $s * 0.9
    $c = [System.Drawing.Color]::FromArgb(120 + $rng.Next(135), 255, 255, 255)
    $g.FillEllipse((New-Object System.Drawing.SolidBrush $c), $rng.Next($Size), $rng.Next($Size), $d, $d)
}

# Planet with a lit edge, then the ring in front.
$cx = 128 * $s; $cy = 132 * $s; $pr = 74 * $s
$planet = New-Object System.Drawing.Drawing2D.GraphicsPath
$planet.AddEllipse($cx - $pr, $cy - $pr, 2 * $pr, 2 * $pr)
$pb = New-Object System.Drawing.Drawing2D.PathGradientBrush $planet
$pb.CenterPoint = New-Object System.Drawing.PointF ($cx - 30 * $s), ($cy - 34 * $s)
$pb.CenterColor = [System.Drawing.Color]::FromArgb(255, 170, 235, 110)
$pb.SurroundColors = @([System.Drawing.Color]::FromArgb(255, 34, 92, 40))
$g.FillPath($pb, $planet)
$ring = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(230, 233, 214, 140)), (9 * $s)
$g.DrawArc($ring, $cx - 112 * $s, $cy - 30 * $s, 224 * $s, 60 * $s, -10, 200)

# X.
$font = New-Object System.Drawing.Font 'Segoe UI Black', (96 * $s), ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
$rect = New-Object System.Drawing.RectangleF 0, (6 * $s), $Size, $Size
$g.DrawString('X', $font, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 0, 0, 0))), (New-Object System.Drawing.RectangleF (4 * $s), (10 * $s), $Size, $Size), $fmt)
$g.DrawString('X', $font, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 245, 240, 220))), $rect, $fmt)

$g.Dispose()
$bmp.Save([IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"Wrote $Out"
