param([Parameter(Mandatory=$true)][string]$Backend,[Parameter(Mandatory=$true)][string]$Scene)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root 'dist\WandouImagePreview.exe'
$output=Join-Path $PSScriptRoot 'smoke.png'
if(Test-Path -LiteralPath $output){Remove-Item -LiteralPath $output}
$p=Start-Process -FilePath $exe -ArgumentList ('--extract "'+$Backend+'" "'+$Scene+'" "'+$output+'"') -PassThru -WindowStyle Hidden
if(-not $p.WaitForExit(12000)){$p.Kill();throw 'Extraction timeout'}
if($p.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $output)){throw 'Extraction failed'}
Add-Type -AssemblyName System.Drawing
$image=[Drawing.Image]::FromFile($output)
try{if($image.Width -lt 1 -or $image.Height -lt 1){throw 'Invalid image size'};Write-Host "PASS: $($image.Width)x$($image.Height)"}finally{$image.Dispose()}
