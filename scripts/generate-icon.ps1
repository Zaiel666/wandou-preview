$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$assetDir=Join-Path $root 'assets'
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
$sizes=@(16,20,24,32,40,48,64,96,128,256)
$payloads=@()
foreach($size in $sizes){
    $bitmap=[Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics=[Drawing.Graphics]::FromImage($bitmap);$graphics.SmoothingMode='AntiAlias';$graphics.Clear([Drawing.Color]::Transparent)
    $scale=$size/64.0
    $top=[Drawing.PointF[]]@((New-Object Drawing.PointF (32*$scale),(5*$scale)),(New-Object Drawing.PointF (57*$scale),(18*$scale)),(New-Object Drawing.PointF (32*$scale),(32*$scale)),(New-Object Drawing.PointF (7*$scale),(18*$scale)))
    $left=[Drawing.PointF[]]@((New-Object Drawing.PointF (7*$scale),(18*$scale)),(New-Object Drawing.PointF (32*$scale),(32*$scale)),(New-Object Drawing.PointF (32*$scale),(59*$scale)),(New-Object Drawing.PointF (7*$scale),(45*$scale)))
    $right=[Drawing.PointF[]]@((New-Object Drawing.PointF (32*$scale),(32*$scale)),(New-Object Drawing.PointF (57*$scale),(18*$scale)),(New-Object Drawing.PointF (57*$scale),(45*$scale)),(New-Object Drawing.PointF (32*$scale),(59*$scale)))
    $topBrush=[Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.PointF]::new(0,0),[Drawing.PointF]::new($size,$size),[Drawing.Color]::FromArgb(255,245,250,255),[Drawing.Color]::FromArgb(255,102,118,138))
    $leftBrush=[Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.PointF]::new(0,0),[Drawing.PointF]::new($size,$size),[Drawing.Color]::FromArgb(255,125,140,158),[Drawing.Color]::FromArgb(255,35,42,53))
    $rightBrush=[Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.PointF]::new(0,0),[Drawing.PointF]::new($size,$size),[Drawing.Color]::FromArgb(255,62,72,86),[Drawing.Color]::FromArgb(255,174,190,205))
    $graphics.FillPolygon($topBrush,$top);$graphics.FillPolygon($leftBrush,$left);$graphics.FillPolygon($rightBrush,$right)
    $pen=New-Object Drawing.Pen ([Drawing.Color]::FromArgb(230,24,30,39)),([Math]::Max(1,1.6*$scale));$graphics.DrawPolygon($pen,$top);$graphics.DrawPolygon($pen,$left);$graphics.DrawPolygon($pen,$right)
    if($size -ge 32){$shine=New-Object Drawing.Pen ([Drawing.Color]::FromArgb(150,255,255,255)),([Math]::Max(1,$scale));$graphics.DrawLine($shine,12*$scale,18*$scale,32*$scale,8*$scale);$shine.Dispose()}
    $stream=New-Object IO.MemoryStream;$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png);$payloads+=,$stream.ToArray()
    if($size -eq 256){$bitmap.Save((Join-Path $assetDir 'metal-cube-preview.png'),[Drawing.Imaging.ImageFormat]::Png)}
    $stream.Dispose();$pen.Dispose();$topBrush.Dispose();$leftBrush.Dispose();$rightBrush.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
$out=New-Object IO.MemoryStream;$writer=New-Object IO.BinaryWriter $out
$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
$offset=6+16*$sizes.Count
for($i=0;$i -lt $sizes.Count;$i++){$v=$sizes[$i];$writer.Write([byte]$(if($v -eq 256){0}else{$v}));$writer.Write([byte]$(if($v -eq 256){0}else{$v}));$writer.Write([byte]0);$writer.Write([byte]0);$writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$payloads[$i].Length);$writer.Write([uint32]$offset);$offset+=$payloads[$i].Length}
foreach($payload in $payloads){$writer.Write($payload)}
[IO.File]::WriteAllBytes((Join-Path $assetDir 'metal-cube.ico'),$out.ToArray());$writer.Dispose();$out.Dispose()
Write-Host "Generated $assetDir\metal-cube.ico"
