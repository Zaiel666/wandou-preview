$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $root 'dist'
$stage=Join-Path $dist 'Wandou-Preview-0.2.0-Windows-x64'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Get-ChildItem -LiteralPath $dist -File | Where-Object {$_.Extension -in @('.exe','.dll')} | Copy-Item -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $root 'assets\metal-cube.ico') -Destination $stage -Force
foreach($name in @('Install.ps1','Uninstall.ps1')){
    # Windows PowerShell 5.1 needs BOM to read Chinese string literals correctly.
    $body=[IO.File]::ReadAllText((Join-Path $root $name))
    [IO.File]::WriteAllText((Join-Path $stage $name),$body,(New-Object Text.UTF8Encoding($true)))
}
foreach($name in @('Install.cmd','Uninstall.cmd','README.md','LICENSE','THIRD-PARTY-NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage -Force}
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $stage -Recurse -Force
Compress-Archive -Path $stage -DestinationPath (Join-Path $dist 'Wandou-Preview-0.2.0-Windows-x64.zip') -Force

# Build a single-file, per-user installer. It embeds the exact same payload as
# the ZIP and runs Install.ps1 invisibly, so installation takes one double-click.
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$setupArgs=@('/nologo','/target:winexe','/platform:x64','/optimize+',('/win32icon:'+ (Join-Path $root 'assets\metal-cube.ico')),('/out:'+ (Join-Path $dist 'Wandou-Preview-0.2.0-Setup.exe')),'/reference:System.Windows.Forms.dll')
foreach($file in @(Get-ChildItem -LiteralPath $stage -File | Where-Object {$_.Extension -in @('.exe','.dll','.ico','.ps1') -or $_.Name -in @('LICENSE','THIRD-PARTY-NOTICES.md')})){$setupArgs+=('/resource:'+ $file.FullName +',WandouPayload.'+ $file.Name)}
$setupArgs+=(Join-Path $root 'src\Setup.cs')
& $compiler $setupArgs
if($LASTEXITCODE -ne 0){throw 'Setup build failed'}
