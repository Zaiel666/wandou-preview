param([string]$Cinema4DPath)
$ErrorActionPreference='Stop'
if(-not [Environment]::Is64BitProcess){throw '请使用 64 位 Windows PowerShell。'}

# Remove this project's older registration before installing the renamed product.
$legacyApp=Join-Path $env:LOCALAPPDATA 'C4DQuickPreview'
foreach($legacy in @(
    @{State=(Join-Path $legacyApp 'registry-backup-v0.2.json');Script=(Join-Path $legacyApp '0.2.0\Uninstall.ps1')},
    @{State=(Join-Path $legacyApp 'registry-backup.json');Script=(Join-Path $legacyApp '0.1.0\Uninstall.ps1')}
)){
    if(Test-Path -LiteralPath $legacy.State){if(-not(Test-Path -LiteralPath $legacy.Script)){throw '检测到旧版注册，但找不到旧版卸载脚本。请先重新安装旧版并卸载。'};& $legacy.Script}
}

$app=Join-Path $env:LOCALAPPDATA 'WandouPreview'
$target=Join-Path $app '0.1.0'
$stateFile=Join-Path $app 'registry-backup.json'
$thumbnailSlot='{e357fccd-a995-4576-b01f-234630154e96}'
$c4dClsid='{3D15370F-5744-41BF-85D8-8D4985509CEE}'
$modelClsid='{4C238E90-C239-4FE9-AD0F-6750784379B2}'
$modelExtensions=@('.3ds','.3mf','.dae','.dxf','.fbx','.glb','.gltf','.ifc','.lwo','.lws','.lxo','.obj','.ply','.stl','.stp','.usd','.usda','.usdc','.usdz','.x','.x3d','.x3db')

foreach($name in @('WandouModelPreview.exe','ModelThumbnail.dll','WandouImagePreview.exe','C4DThumbnail.dll','metal-cube.ico')){
    if(-not(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))){throw "安装包不完整：缺少 $name。请下载 Releases 中的 ZIP，不要下载 Source code。"}
}

$candidates=@()
if($Cinema4DPath){
    if([IO.Path]::GetExtension($Cinema4DPath) -eq '.dll'){$candidates+=Get-Item -LiteralPath $Cinema4DPath}
    else{$candidate=Join-Path $Cinema4DPath 'resource\libs\win64\win_thumbnail.dll';if(Test-Path -LiteralPath $candidate){$candidates+=Get-Item -LiteralPath $candidate}}
} else {
    foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)})){
        if(-not $base){continue}
        foreach($dir in @(Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue | Where-Object Name -Match 'Cinema 4D' | Sort-Object Name -Descending)){
            $candidate=Join-Path $dir.FullName 'resource\libs\win64\win_thumbnail.dll';if(Test-Path -LiteralPath $candidate){$candidates+=Get-Item -LiteralPath $candidate}
        }
    }
}
$backend=if($candidates.Count){$candidates[0].FullName}else{$null}

New-Item -ItemType Directory -Force -Path $target | Out-Null
foreach($from in @(Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {$_.Extension -in @('.exe','.dll','.ico') -or $_.Name -eq 'Uninstall.ps1'})){
    $to=Join-Path $target $from.Name
    if((Test-Path -LiteralPath $to) -and ((Get-FileHash -LiteralPath $from.FullName).Hash -eq (Get-FileHash -LiteralPath $to).Hash)){continue}
    Copy-Item -LiteralPath $from.FullName -Destination $to -Force
}

$icon=Join-Path $target 'metal-cube.ico';$modelExe=Join-Path $target 'WandouModelPreview.exe';$imageExe=Join-Path $target 'WandouImagePreview.exe'
$changes=@(
    @{Path="Software\Classes\CLSID\$modelClsid";Name='';Value='Wandou Preview Thumbnail'},
    @{Path="Software\Classes\CLSID\$modelClsid";Name='DisableProcessIsolation';Value=1;Kind='DWord'},
    @{Path="Software\Classes\CLSID\$modelClsid\InprocServer32";Name='';Value=(Join-Path $target 'ModelThumbnail.dll')},
    @{Path="Software\Classes\CLSID\$modelClsid\InprocServer32";Name='ThreadingModel';Value='Apartment'}
)
foreach($extension in $modelExtensions){
    $changes+=@{Path="Software\Classes\$extension\shellex\$thumbnailSlot";Name='';Value=$modelClsid}
    $verb="Software\Classes\SystemFileAssociations\$extension\shell\WandouPreview"
    $changes+=@{Path=$verb;Name='';Value='3D 模型快速预览'}
    $changes+=@{Path=$verb;Name='Icon';Value=$icon}
    $changes+=@{Path="$verb\command";Name='';Value=('"'+$modelExe+'" "%1"')}
}

# Illustrator files saved with PDF compatibility can be rendered without Illustrator.
$changes+=@(
    @{Path="Software\Classes\.ai\shellex\$thumbnailSlot";Name='';Value=$modelClsid},
    @{Path='Software\Classes\SystemFileAssociations\.ai\shell\WandouPreview';Name='';Value='豌豆预览（AI）'},
    @{Path='Software\Classes\SystemFileAssociations\.ai\shell\WandouPreview';Name='Icon';Value=$icon},
    @{Path='Software\Classes\SystemFileAssociations\.ai\shell\WandouPreview\command';Name='';Value=('"'+$imageExe+'" "%1"')}
)
$changes+=@(
    @{Path="Software\Classes\.hdr\shellex\$thumbnailSlot";Name='';Value=$modelClsid},
    @{Path='Software\Classes\SystemFileAssociations\.hdr\shell\WandouPreview';Name='';Value='豌豆预览（HDR）'},
    @{Path='Software\Classes\SystemFileAssociations\.hdr\shell\WandouPreview';Name='Icon';Value=$icon},
    @{Path='Software\Classes\SystemFileAssociations\.hdr\shell\WandouPreview\command';Name='';Value=('"'+$imageExe+'" "%1"')}
)

if($backend){
    [IO.File]::WriteAllText((Join-Path $target 'backend.txt'),$backend,(New-Object Text.UTF8Encoding($false)))
    $changes+=@(
        @{Path='Software\WandouPreview';Name='C4DBackend';Value=$backend},
        @{Path="Software\Classes\CLSID\$c4dClsid";Name='';Value='Wandou Preview C4D Thumbnail'},
        @{Path="Software\Classes\CLSID\$c4dClsid";Name='DisableProcessIsolation';Value=1;Kind='DWord'},
        @{Path="Software\Classes\CLSID\$c4dClsid\InprocServer32";Name='';Value=(Join-Path $target 'C4DThumbnail.dll')},
        @{Path="Software\Classes\CLSID\$c4dClsid\InprocServer32";Name='ThreadingModel';Value='Apartment'},
        @{Path="Software\Classes\.c4d\shellex\$thumbnailSlot";Name='';Value=$c4dClsid},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\WandouPreview';Name='';Value='豌豆预览（C4D）'},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\WandouPreview';Name='Icon';Value=$icon},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\WandouPreview\command';Name='';Value=('"'+$imageExe+'" "%1"')}
    )
    $extensionKey=[Microsoft.Win32.Registry]::ClassesRoot.OpenSubKey('.c4d')
    if($extensionKey){$progId=$extensionKey.GetValue('');$extensionKey.Close();if($progId){$changes+=@{Path="Software\Classes\$progId\shellex\$thumbnailSlot";Name='';Value=$c4dClsid}}}
}

$backup=@();if(Test-Path -LiteralPath $stateFile){$backup=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json}
foreach($change in $changes){
    $prior=@($backup | Where-Object {$_.Path -eq $change.Path -and $_.Name -eq $change.Name})
    if($prior.Count){if($prior[0].Installed -ne $change.Value){throw '已安装的版本使用了不同设置，请先运行 Uninstall.cmd。'};continue}
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($change.Path);$exists=$false;$value=$null;$kind='String'
    if($key){$exists=$key.GetValueNames() -contains $change.Name;if($exists){$value=$key.GetValue($change.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);$kind=$key.GetValueKind($change.Name).ToString()};$key.Close()}
    $backup+=[pscustomobject]@{Path=$change.Path;Name=$change.Name;Exists=$exists;Value=$value;Kind=$kind;Installed=$change.Value}
}
$backup | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $stateFile -Encoding UTF8
try{
    foreach($change in $changes){$key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($change.Path);try{$kind=[Microsoft.Win32.RegistryValueKind]::String;if($change.Kind){$kind=[Microsoft.Win32.RegistryValueKind]::$($change.Kind)};$key.SetValue($change.Name,$change.Value,$kind)}finally{$key.Close()}}
}catch{$originalError=$_;& (Join-Path $target 'Uninstall.ps1');throw $originalError}

Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class WandouShellNotify{[DllImport("shell32.dll")]public static extern void SHChangeNotify(uint e,uint f,IntPtr a,IntPtr b);}'
[WandouShellNotify]::SHChangeNotify(0x08000000,0x1000,[IntPtr]::Zero,[IntPtr]::Zero)
Write-Host '豌豆预览 0.1.0 安装完成。' -ForegroundColor Green
Write-Host '文件夹切换到“大图标”或“超大图标”即可查看缩略图；右键模型可旋转、缩放。'
Write-Host 'Windows 11 首次使用可能需要点“显示更多选项”。'
if($backend){Write-Host "已启用 C4D 保存预览：$backend"}else{Write-Host '未找到 Cinema 4D；FBX、OBJ 等通用模型和 AI 预览仍可使用。' -ForegroundColor Yellow}
Write-Host '卸载时双击同一文件夹中的 Uninstall.cmd。'
