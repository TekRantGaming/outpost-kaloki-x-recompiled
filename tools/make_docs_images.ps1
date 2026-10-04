# Builds the README images in docs/images from 4K screenshots in screenshots/.
param([string]$Src = "$PSScriptRoot\..\screenshots", [string]$Dst = "$PSScriptRoot\..\docs\images")
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $Dst | Out-Null
$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$params = New-Object System.Drawing.Imaging.EncoderParameters 1
$params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]88)

function Save-Scaled($in, $out, $width, [System.Drawing.Rectangle]$crop = [System.Drawing.Rectangle]::Empty) {
  $img = [System.Drawing.Image]::FromFile($in)
  if ($crop.IsEmpty) { $crop = New-Object System.Drawing.Rectangle 0, 0, $img.Width, $img.Height }
  $h = [int]($crop.Height * $width / $crop.Width)
  $bmp = New-Object System.Drawing.Bitmap $width, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $width, $h), $crop, [System.Drawing.GraphicsUnit]::Pixel)
  $bmp.Save($out, $jpeg, $params)
  $g.Dispose(); $bmp.Dispose(); $img.Dispose()
}

# Banner: the launcher's header band.
Save-Scaled "$Src\launcher_play.png" "$Dst\banner.jpg" 1920 (New-Object System.Drawing.Rectangle 0, 0, 3840, 420)
$map = [ordered]@{
  'launcher_play' = 'launcher-play'; 'launcher_display' = 'launcher-display'; 'launcher_graphics' = 'launcher-graphics'
  'launcher_gameplay' = 'launcher-gameplay'; 'launcher_controls' = 'launcher-controls'
  'launcher_achievements' = 'launcher-achievements'; 'launcher_about' = 'launcher-about'
  'shot_menu' = 'game-menu'; 'shot_story' = 'game-story'; 'shot_chapter' = 'game-chapter'
  'shot_station' = 'game-station'; 'shot_boss' = 'game-boss'; 'shot_build' = 'game-build'
  'shot_building' = 'game-building'; 'welcome_toast' = 'achievement-popup'; 'v4_sounds' = 'launcher-sounds'
}
foreach ($k in $map.Keys) { Save-Scaled "$Src\$k.png" "$Dst\$($map[$k]).jpg" 1600 }
Get-ChildItem $Dst | Select-Object Name, @{n='KB';e={[int]($_.Length/1KB)}} | Format-Table -AutoSize | Out-String
