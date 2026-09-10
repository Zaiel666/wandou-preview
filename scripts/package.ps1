$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $root 'dist'
$stage=Join-Path $dist 'C4D-QuickPreview-0.1.0-Windows-x64'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach($name in @('C4DQuickPreview.exe','C4DThumbnail.dll')){Copy-Item -LiteralPath (Join-Path $dist $name) -Destination $stage -Force}
foreach($name in @('Install.ps1','Uninstall.ps1')){
    # Windows PowerShell 5.1 needs BOM to read Chinese string literals correctly.
    $body=[IO.File]::ReadAllText((Join-Path $root $name))
    [IO.File]::WriteAllText((Join-Path $stage $name),$body,(New-Object Text.UTF8Encoding($true)))
}
foreach($name in @('Install.cmd','Uninstall.cmd','README.md','LICENSE')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage -Force}
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $stage -Recurse -Force
Compress-Archive -Path $stage -DestinationPath (Join-Path $dist 'C4D-QuickPreview-0.1.0-Windows-x64.zip') -Force
