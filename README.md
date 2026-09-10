# C4D Quick Preview · C4D 快速预览

Windows 10/11 x64 的 C4D 场景缩略图试用工具。文件夹直接显示场景保存的预览图片，左下角显示小型 C4D 文字标识；右键可打开图片预览窗口。

**这是 0.1.0 C4D 验证版，不是完整三维查看器。** 预览窗口支持滚轮缩放、拖动平移、适合窗口，不支持旋转三维场景。HDR、Blender、FBX、OBJ 等格式尚未加入本版。QuickLook 和空格快捷键不受影响。

## 安装

1. 在本仓库 **Releases** 下载 `C4D-QuickPreview-0.1.0-Windows-x64.zip`。不要下载 GitHub 自动提供的 Source code。
2. 把 ZIP 完整解压到一个文件夹。
3. 双击 `Install.cmd`，普通用户运行即可，无需管理员权限。
4. 打开含 `.c4d` 文件的文件夹，将查看方式切换为 **大图标** 或 **超大图标**。
5. 右键 `.c4d` → **C4D 快速预览**。Windows 11 的传统扩展菜单可能位于 **显示更多选项** 内。

详细说明和故障排查：[安装与使用](docs/安装与使用.md)。

## 依赖及兼容范围

- 本机必须已有可加载的 `resource\libs\win64\win_thumbnail.dll`（来自用户自己的 Cinema 4D 安装）。不需要另外配置 SDK，也不需要打开 C4D 主程序。
- 安装器会在 Program Files 下的 Cinema 4D 目录查找组件。自定义位置见安装文档。
- 目前实际读取测试使用 Cinema 4D 2023 自带组件与示例场景。其他版本、第三方场景、没有保存预览的文件仍需测试。
- 不是所有 C4D 安装都附带此 DLL。本机 2024 的标准位置没有找到它，2023 有；不代表所有 2024 安装情况相同。
- 不分发 Maxon DLL、C4D 程序、用户场景或官方示例场景。删除或移动本机 C4D 组件后，需要重新安装并指定有效组件。
- Windows 自带 .NET Framework 4.x 用于图片预览窗口；缩略图扩展为原生 x64 DLL。

## 大场景

读取文件中已有的图片，不计算场景最终渲染，也不主动载入几何数据进行渲染。具体读取量仍由本机 C4D 组件决定，不能保证每个版本对所有大文件都只读固定大小的数据。

资源管理器使用 Windows 缩略图缓存和默认的进程隔离机制；右键窗口使用独立提取进程，12 秒超时终止。右键图片缓存由文件路径、大小、修改时间和组件版本时间戳生成键。缓存尚无自动容量清理，可手动删除 `%LOCALAPPDATA%\C4DQuickPreview\Cache`。

## 卸载

双击解压目录的 `Uninstall.cmd`。程序恢复被替换的当前用户注册表值。如果值后来被其他程序修改，会保留其他程序的值。

卸载不会停止资源管理器、关闭用户文件或清空系统缩略图缓存。程序及图片缓存保留在 `%LOCALAPPDATA%\C4DQuickPreview`；退出预览并注销 Windows 后可以手动删除。

## 开发

`scripts/build.ps1` 使用 Windows 的 .NET Framework C# 编译器构建窗口。原生适配器使用 MSVC：

```bat
cl /nologo /LD /O2 /MT /EHsc src\Thumbnail.cpp /link /OUT:dist\C4DThumbnail.dll ole32.lib gdi32.lib user32.lib advapi32.lib uuid.lib
```

然后运行 `scripts/package.ps1`。GitHub Actions 自动完成两部分编译和打包。

本工具源码采用 MIT 许可。Cinema 4D 与 Maxon 商标及组件归各自权利人所有；本项目不是 Maxon 官方产品。
