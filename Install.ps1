param([string]$Cinema4DPath)
$ErrorActionPreference='Stop'
if(-not [Environment]::Is64BitProcess){throw 'Please run the 64-bit Windows PowerShell.'}
$app=Join-Path $env:LOCALAPPDATA 'C4DQuickPreview'
$target=Join-Path $app '0.1.0'
$stateFile=Join-Path $app 'registry-backup.json'
$clsid='{3D15370F-5744-41BF-85D8-8D4985509CEE}'
$handler='Software\Classes\.c4d\shellex\{e357fccd-a995-4576-b01f-234630154e96}'
$candidates=@()
if($Cinema4DPath){
    if([IO.Path]::GetExtension($Cinema4DPath) -eq '.dll'){$candidates+=Get-Item -LiteralPath $Cinema4DPath}
    else{$candidate=Join-Path $Cinema4DPath 'resource\libs\win64\win_thumbnail.dll';if(Test-Path -LiteralPath $candidate){$candidates+=Get-Item -LiteralPath $candidate}}
} else {
    foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)})){
        if(-not $base){continue}
        foreach($dir in @(Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue | Where-Object Name -Match 'Cinema 4D' | Sort-Object Name -Descending)){
            $candidate=Join-Path $dir.FullName 'resource\libs\win64\win_thumbnail.dll'
            if(Test-Path -LiteralPath $candidate){$candidates+=Get-Item -LiteralPath $candidate}
        }
    }
}
if(-not $candidates.Count){throw 'No C4D thumbnail DLL found. Run Install.ps1 -Cinema4DPath "D:\Your Cinema 4D folder" (or pass the full win_thumbnail.dll path).'}
$backend=$candidates[0].FullName
foreach($name in @('C4DQuickPreview.exe','C4DThumbnail.dll')){if(-not(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))){throw "Package is incomplete: $name. Download the release ZIP, not Source code."}}
New-Item -ItemType Directory -Force -Path $target | Out-Null
foreach($name in @('C4DQuickPreview.exe','C4DThumbnail.dll','Uninstall.ps1')){
    $from=Join-Path $PSScriptRoot $name;$to=Join-Path $target $name
    if((Test-Path -LiteralPath $to) -and ((Get-FileHash -LiteralPath $from).Hash -eq (Get-FileHash -LiteralPath $to).Hash)){continue}
    Copy-Item -LiteralPath $from -Destination $to -Force
}
[IO.File]::WriteAllText((Join-Path $target 'backend.txt'),$backend,(New-Object Text.UTF8Encoding($false)))
$changes=@(
    @{Path='Software\C4DQuickPreview';Name='Backend';Value=$backend},
    @{Path="Software\Classes\CLSID\$clsid";Name='';Value='C4D Quick Preview Thumbnail'},
    @{Path="Software\Classes\CLSID\$clsid";Name='DisableProcessIsolation';Value=1;Kind='DWord'},
    @{Path="Software\Classes\CLSID\$clsid\InprocServer32";Name='';Value=(Join-Path $target 'C4DThumbnail.dll')},
    @{Path="Software\Classes\CLSID\$clsid\InprocServer32";Name='ThreadingModel';Value='Apartment'},
    @{Path=$handler;Name='';Value=$clsid},
    @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\C4DQuickPreview';Name='';Value='C4D 快速预览'},
    @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\C4DQuickPreview\command';Name='';Value=('"'+(Join-Path $target 'C4DQuickPreview.exe')+'" "%1"')}
)
$extensionKey=[Microsoft.Win32.Registry]::ClassesRoot.OpenSubKey('.c4d')
if($extensionKey){$progId=$extensionKey.GetValue('');$extensionKey.Close();if($progId){$changes+=@{Path="Software\Classes\$progId\shellex\{e357fccd-a995-4576-b01f-234630154e96}";Name='';Value=$clsid}}}
$backup=@()
if(Test-Path -LiteralPath $stateFile){$backup=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json}
foreach($change in $changes){
    $prior=@($backup | Where-Object {$_.Path -eq $change.Path -and $_.Name -eq $change.Name})
    if($prior.Count){if($prior[0].Installed -ne $change.Value){throw 'An installation with a different backend already exists. Uninstall it first.'};continue}
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($change.Path)
    $exists=$false;$value=$null;$kind='String'
    if($key){$exists=$key.GetValueNames() -contains $change.Name;if($exists){$value=$key.GetValue($change.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);$kind=$key.GetValueKind($change.Name).ToString()};$key.Close()}
    $backup+= [pscustomobject]@{Path=$change.Path;Name=$change.Name;Exists=$exists;Value=$value;Kind=$kind;Installed=$change.Value}
}
$backup | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $stateFile -Encoding UTF8
try {
    foreach($change in $changes){$key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($change.Path);try{$kind=[Microsoft.Win32.RegistryValueKind]::String;if($change.Kind){$kind=[Microsoft.Win32.RegistryValueKind]::$($change.Kind)};$key.SetValue($change.Name,$change.Value,$kind)}finally{$key.Close()}}
} catch {
    $originalError=$_
    & (Join-Path $target 'Uninstall.ps1')
    throw $originalError
}
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class ShellNotify{[DllImport("shell32.dll")]public static extern void SHChangeNotify(uint e,uint f,IntPtr a,IntPtr b);}'
[ShellNotify]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
Write-Host "安装完成。组件：$backend" -ForegroundColor Green
Write-Host '文件夹切换到大图标/超大图标查看 .c4d；右键选择 C4D 快速预览。Windows 11 可能需要点“显示更多选项”。'
Write-Host '此版本读取保存的场景图片，支持图片缩放和平移，不支持三维旋转。未修改空格键或默认打开软件。'
Write-Host "卸载：运行解压目录的 Uninstall.cmd。"
