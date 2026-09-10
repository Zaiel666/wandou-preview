$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /win32icon:"$root\assets\metal-cube.ico" /reference:System.Drawing.dll /reference:System.Windows.Forms.dll "/out:$out\WandouImagePreview.exe" "$root\src\Preview.cs"
if($LASTEXITCODE -ne 0){throw 'C# build failed'}
Write-Host "Built $out\WandouImagePreview.exe"
