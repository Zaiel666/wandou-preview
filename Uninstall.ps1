$ErrorActionPreference='Stop'
$app=Join-Path $env:LOCALAPPDATA 'WandouPreview'
$stateFile=Join-Path $app 'registry-backup.json'
if(-not(Test-Path -LiteralPath $stateFile)){Write-Host '没有找到豌豆预览的安装记录。';exit 0}
$backup=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
foreach($entry in $backup){
    if(-not($entry.Path -like 'Software\Classes\*' -or $entry.Path -eq 'Software\WandouPreview' -or $entry.Path -eq 'Software\Microsoft\Windows\CurrentVersion\Run')){throw '安装备份包含意外的注册表路径。'}
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Path,$true);if(-not $key){continue}
    try{$current=$key.GetValue($entry.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);if($current -ne $entry.Installed){Write-Host "保留其他软件后来修改的设置：$($entry.Path)";continue};if($entry.Exists){$kind=[Microsoft.Win32.RegistryValueKind]::$($entry.Kind);$key.SetValue($entry.Name,$entry.Value,$kind)}else{$key.DeleteValue($entry.Name,$false)}}finally{$key.Close()}
}
foreach($entry in @($backup | Sort-Object {$_.Path.Length} -Descending)){$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Path);if($key){$empty=$key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0;$key.Close();if($empty){[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($entry.Path,$false)}}}
Remove-Item -LiteralPath $stateFile
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class WandouShellRemoveNotify{[DllImport("shell32.dll")]public static extern void SHChangeNotify(uint e,uint f,IntPtr a,IntPtr b);}'
[WandouShellRemoveNotify]::SHChangeNotify(0x08000000,0x1000,[IntPtr]::Zero,[IntPtr]::Zero)
Write-Host '豌豆预览已卸载，原来的文件关联已恢复。' -ForegroundColor Green
Write-Host "程序缓存保留在 $app；注销 Windows 后可以手动删除此目录。"
