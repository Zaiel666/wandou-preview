param([Parameter(Mandatory=$true)][string]$Package,[Parameter(Mandatory=$true)][string]$Scene)
$ErrorActionPreference='Stop'
$app=Join-Path $env:LOCALAPPDATA 'C4DQuickPreview'
$state=Join-Path $app 'registry-backup.json'
if(Test-Path -LiteralPath $state){throw 'An existing installation must not be overwritten by this test.'}
$root=Split-Path $PSScriptRoot -Parent
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:exe /platform:x64 /reference:System.Drawing.dll "/out:$root\dist\ShellSmoke.exe" "$PSScriptRoot\ShellSmoke.cs"
if($LASTEXITCODE -ne 0){throw 'Test build failed'}
$saved=@()
try{
    & (Join-Path $Package 'Install.ps1')
    $saved=Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
    $output=Join-Path $PSScriptRoot 'shell-preview.png'
    $p=Start-Process -FilePath "$root\dist\ShellSmoke.exe" -ArgumentList ('"'+$Scene+'" "'+$output+'"') -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput "$root\dist\shell-test.log"
    Get-Content "$root\dist\shell-test.log"
    if($p.ExitCode -ne 0){throw 'Shell thumbnail extraction failed'}
    Write-Host 'PASS: Windows Shell returned a thumbnail.'
}finally{
    if(Test-Path -LiteralPath $state){& (Join-Path $Package 'Uninstall.ps1')}
}
foreach($entry in $saved){
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Path)
    try{
        if($entry.Exists){if(-not $key -or $key.GetValue($entry.Name) -ne $entry.Value){throw "Restoration failed: $($entry.Path)"}}
        elseif($key -and ($key.GetValueNames() -contains $entry.Name)){throw "Registration remained after uninstall: $($entry.Path)"}
    }finally{if($key){$key.Close()}}
}
Write-Host 'PASS: Uninstall restored all prior registry values.'
