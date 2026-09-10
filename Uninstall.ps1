$ErrorActionPreference='Stop'
$app=Join-Path $env:LOCALAPPDATA 'C4DQuickPreview'
$stateFile=Join-Path $app 'registry-backup.json'
if(-not(Test-Path -LiteralPath $stateFile)){Write-Host 'No installation backup found; nothing changed.';exit 0}
$backup=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
foreach($entry in $backup){
    if(-not($entry.Path -like 'Software\Classes\*' -or $entry.Path -eq 'Software\C4DQuickPreview')){throw 'Unexpected registry path in backup.'}
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Path,$true)
    if(-not $key){continue}
    try{
        $current=$key.GetValue($entry.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        if($current -ne $entry.Installed){Write-Host "Preserved a value changed by another application: $($entry.Path)";continue}
        if($entry.Exists){$kind=[Microsoft.Win32.RegistryValueKind]::$($entry.Kind);$key.SetValue($entry.Name,$entry.Value,$kind)}else{$key.DeleteValue($entry.Name,$false)}
    }finally{$key.Close()}
}
# Remove only empty keys touched by this package. Never remove a foreign subtree.
foreach($entry in @($backup | Sort-Object { $_.Path.Length } -Descending)){
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Path)
    if($key){$empty=$key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0;$key.Close();if($empty){[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($entry.Path,$false)}}
}
Remove-Item -LiteralPath $stateFile
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class ShellRemoveNotify{[DllImport("shell32.dll")]public static extern void SHChangeNotify(uint e,uint f,IntPtr a,IntPtr b);}'
[ShellRemoveNotify]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
Write-Host '已卸载缩略图和右键菜单注册，并恢复原来的设置。' -ForegroundColor Green
Write-Host "程序和缓存保留在 $app；退出预览并注销 Windows 后，可以手动删除此目录。"
