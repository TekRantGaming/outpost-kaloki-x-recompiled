# Composes docs/images/banner.jpg: the game's logo (from a main-menu screenshot)
# over the title-screen space scene, with the port's tagline.
param(
  [string]$Menu = "$PSScriptRoot\..\screenshots\shot_menu.png",
  [string]$Backdrop = "$env:USERPROFILE\Documents\outpost_kaloki_x\launcher\title.bmp",
  [string]$Out = "$PSScriptRoot\..\docs\images\banner.jpg"
)
Add-Type -AssemblyName System.Drawing
$W = 1920; $H = 380
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

# Backdrop: title screen scaled to the banner width, band from just above the planet.
$bg = [System.Drawing.Image]::FromFile($Backdrop)
$scale = $W / $bg.Width
$srcH = [int]($H / $scale)
$g.DrawImage($bg, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), (New-Object System.Drawing.Rectangle 0, 0, $bg.Width, $srcH), [System.Drawing.GraphicsUnit]::Pixel)
$bg.Dispose()

# Darken the left for the logo and fade the edges.
$shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point ([int]($W * 0.7)), 0), ([System.Drawing.Color]::FromArgb(200, 7, 10, 22)), ([System.Drawing.Color]::FromArgb(0, 7, 10, 22))
$g.FillRectangle($shade, 0, 0, [int]($W * 0.7), $H)

# Logo from the main menu (4K screenshot).
Add-Type -ReferencedAssemblies System.Drawing @'
using System.Drawing;
using System.Drawing.Imaging;
public static class LogoKey {
    // Copies `src` out of `img` with near-black (space background) pixels faded
    // to transparent, so the logo blends into the banner backdrop.
    public static Bitmap Cut(Image img, Rectangle src) {
        var outBmp = new Bitmap(src.Width, src.Height, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(outBmp)) g.DrawImage(img, new Rectangle(0, 0, src.Width, src.Height), src, GraphicsUnit.Pixel);
        var d = outBmp.LockBits(new Rectangle(0, 0, src.Width, src.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var px = new int[src.Width * src.Height];
        System.Runtime.InteropServices.Marshal.Copy(d.Scan0, px, 0, px.Length);
        for (int i = 0; i < px.Length; i++) {
            int c = px[i], r = (c >> 16) & 255, gg = (c >> 8) & 255, b = c & 255;
            int lum = (r * 3 + gg * 6 + b) / 10;
            int a = lum < 45 ? 0 : lum > 85 ? 255 : (lum - 45) * 255 / 40;
            px[i] = (a << 24) | (c & 0xFFFFFF);
        }
        System.Runtime.InteropServices.Marshal.Copy(px, 0, d.Scan0, px.Length);
        outBmp.UnlockBits(d);
        return outBmp;
    }
}
'@
$menuSrc = [System.Drawing.Image]::FromFile($Menu)
$menuImg = [LogoKey]::Cut($menuSrc, (New-Object System.Drawing.Rectangle 600, 312, 1225, 495))
$menuSrc.Dispose()
$logoSrc = New-Object System.Drawing.Rectangle 0, 0, 1225, 495
$logoH = 300; $logoW = [int]($logoSrc.Width * $logoH / $logoSrc.Height)
$g.DrawImage($menuImg, (New-Object System.Drawing.Rectangle 110, 40, $logoW, $logoH), $logoSrc, [System.Drawing.GraphicsUnit]::Pixel)
$menuImg.Dispose()

# Tagline.
$tx = 110 + $logoW + 70
$big = New-Object System.Drawing.Font "Segoe UI", 54, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$small = New-Object System.Drawing.Font "Segoe UI Semibold", 30, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(160, 0, 0, 0))
$white = [System.Drawing.Brushes]::White
$green = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 139, 213, 80))
$g.DrawString("Native PC port", $big, $shadow, $tx + 3, 133)
$g.DrawString("Native PC port", $big, $white, $tx, 130)
$g.DrawString("of the Xbox 360 edition", $small, $green, $tx + 4, 202)

$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$p = New-Object System.Drawing.Imaging.EncoderParameters 1
$p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]90)
$bmp.Save($Out, $jpeg, $p)
$g.Dispose(); $bmp.Dispose()
"banner: $Out"
