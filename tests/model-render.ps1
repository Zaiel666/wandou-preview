param(
    [Parameter(Mandatory=$true)][string]$Viewer,
    [Parameter(Mandatory=$true)][string[]]$Models
)
$ErrorActionPreference='Stop'
$outputDir=Join-Path $PSScriptRoot 'model-output'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
foreach($model in $Models){
    if(-not(Test-Path -LiteralPath $model)){throw "Model not found: $model"}
    $name=[IO.Path]::GetFileNameWithoutExtension($model)
    $safe=[regex]::Replace($name,'[^\p{L}\p{N}_-]','_')
    $output=Join-Path $outputDir ($safe+'.bmp')
    $arguments='--render "'+$model+'" "'+$output+'" 512'
    $process=Start-Process -FilePath $Viewer -ArgumentList $arguments -PassThru -Wait -WindowStyle Hidden
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $output)){throw "Render failed ($($process.ExitCode)): $model"}
    Add-Type -AssemblyName System.Drawing
    $image=[Drawing.Image]::FromFile($output)
    try{if($image.Width -ne 512 -or $image.Height -ne 512){throw "Unexpected image size: $($image.Width)x$($image.Height)"}}finally{$image.Dispose()}
    Write-Host "PASS: $([IO.Path]::GetExtension($model)) -> $output"
}
