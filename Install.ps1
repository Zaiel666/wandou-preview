param([string]$Cinema4DPath)
$ErrorActionPreference='Stop'
if(-not [Environment]::Is64BitProcess){throw 'Please run the 64-bit Windows PowerShell.'}
$app=Join-Path $env:LOCALAPPDATA 'C4DQuickPreview'
$target=Join-Path $app '0.2.0'
$stateFile=Join-Path $app 'registry-backup-v0.2.json'
$thumbnailSlot='{e357fccd-a995-4576-b01f-234630154e96}'
$c4dClsid='{3D15370F-5744-41BF-85D8-8D4985509CEE}'
$modelClsid='{4C238E90-C239-4FE9-AD0F-6750784379B2}'
$modelExtensions=@('.3ds','.3mf','.ac','.ac3d','.ase','.b3d','.bvh','.cob','.csm','.dae','.dxf','.fbx','.glb','.gltf','.ifc','.iqm','.irr','.irrmesh','.lwo','.lws','.lxo','.m3d','.md2','.md3','.md5mesh','.mdl','.ms3d','.ndo','.nff','.obj','.off','.ogex','.ply','.pmx','.q3o','.q3s','.raw','.scn','.sib','.smd','.stl','.stp','.ter','.uc','.usd','.usda','.usdc','.usdz','.vta','.x','.x3d','.x3db','.xgl','.zgl')
$oldState=Join-Path $app 'registry-backup.json'
if(Test-Path -LiteralPath $oldState){
    $oldUninstall=Join-Path $app '0.1.0\Uninstall.ps1'
    if(-not(Test-Path -LiteralPath $oldUninstall)){throw 'Found version 0.1.0 registration but its uninstaller is missing. Reinstall 0.1.0, uninstall it, then install this version.'}
    & $oldUninstall
}
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
$backend=if($candidates.Count){$candidates[0].FullName}else{$null}
foreach($name in @('ModelQuickPreview.exe','ModelThumbnail.dll','C4DQuickPreview.exe','C4DThumbnail.dll','metal-cube.ico')){if(-not(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))){throw "Package is incomplete: $name. Download the release ZIP, not Source code."}}
New-Item -ItemType Directory -Force -Path $target | Out-Null
foreach($from in @(Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {$_.Extension -in @('.exe','.dll','.ico') -or $_.Name -eq 'Uninstall.ps1'})){
    $to=Join-Path $target $from.Name
    if((Test-Path -LiteralPath $to) -and ((Get-FileHash -LiteralPath $from).Hash -eq (Get-FileHash -LiteralPath $to).Hash)){continue}
    Copy-Item -LiteralPath $from.FullName -Destination $to -Force
}
$icon=Join-Path $target 'metal-cube.ico'
$changes=@(
    @{Path="Software\Classes\CLSID\$modelClsid";Name='';Value='3D Model Quick Preview Thumbnail'},
    @{Path="Software\Classes\CLSID\$modelClsid";Name='DisableProcessIsolation';Value=1;Kind='DWord'},
    @{Path="Software\Classes\CLSID\$modelClsid\InprocServer32";Name='';Value=(Join-Path $target 'ModelThumbnail.dll')},
    @{Path="Software\Classes\CLSID\$modelClsid\InprocServer32";Name='ThreadingModel';Value='Apartment'}
)
foreach($extension in $modelExtensions){
    $changes+=@{Path="Software\Classes\$extension\shellex\$thumbnailSlot";Name='';Value=$modelClsid}
    $verb="Software\Classes\SystemFileAssociations\$extension\shell\ModelQuickPreview"
    $changes+=@{Path=$verb;Name='';Value='3D 模型快速预览'}
    $changes+=@{Path=$verb;Name='Icon';Value=$icon}
    $changes+=@{Path="$verb\command";Name='';Value=('"'+(Join-Path $target 'ModelQuickPreview.exe')+'" "%1"')}
}
if($backend){
    [IO.File]::WriteAllText((Join-Path $target 'backend.txt'),$backend,(New-Object Text.UTF8Encoding($false)))
    $changes+=@(
        @{Path='Software\C4DQuickPreview';Name='Backend';Value=$backend},
        @{Path="Software\Classes\CLSID\$c4dClsid";Name='';Value='C4D Quick Preview Thumbnail'},
        @{Path="Software\Classes\CLSID\$c4dClsid";Name='DisableProcessIsolation';Value=1;Kind='DWord'},
        @{Path="Software\Classes\CLSID\$c4dClsid\InprocServer32";Name='';Value=(Join-Path $target 'C4DThumbnail.dll')},
        @{Path="Software\Classes\CLSID\$c4dClsid\InprocServer32";Name='ThreadingModel';Value='Apartment'},
        @{Path="Software\Classes\.c4d\shellex\$thumbnailSlot";Name='';Value=$c4dClsid},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\ModelQuickPreview';Name='';Value='3D 模型快速预览'},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\ModelQuickPreview';Name='Icon';Value=$icon},
        @{Path='Software\Classes\SystemFileAssociations\.c4d\shell\ModelQuickPreview\command';Name='';Value=('"'+(Join-Path $target 'C4DQuickPreview.exe')+'" "%1"')}
    )
    $extensionKey=[Microsoft.Win32.Registry]::ClassesRoot.OpenSubKey('.c4d')
    if($extensionKey){$progId=$extensionKey.GetValue('');$extensionKey.Close();if($progId){$changes+=@{Path="Software\Classes\$progId\shellex\$thumbnailSlot";Name='';Value=$c4dClsid}}}
}
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
Write-Host '安装完成：通用模型缩略图和“3D 模型快速预览”右键菜单已启用。' -ForegroundColor Green
if($backend){Write-Host "C4D 场景图片组件：$backend"}else{Write-Host '没有找到 C4D 缩略图组件；通用模型功能不受影响。如需 C4D，请用 -Cinema4DPath 指定安装目录。' -ForegroundColor Yellow}
Write-Host '文件夹切换到大图标/超大图标查看。Windows 11 右键菜单可能需要点“显示更多选项”。'
Write-Host '通用模型窗口支持旋转、平移和缩放；C4D 当前显示保存时的二维场景图片。未修改空格键或默认打开软件。'
Write-Host "卸载：运行解压目录的 Uninstall.cmd。"
