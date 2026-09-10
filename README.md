# 3D 模型快速预览 · Model Quick Preview

Windows 10/11 x64 的文件夹三维模型缩略图和右键交互预览工具。它不占用空格键，因此可以和 QuickLook 同时使用。

![金属立方体图标](assets/metal-cube-preview.png)

## 0.2.0 提供的功能

- 文件资源管理器直接显示通用三维模型缩略图，左下角标注 FBX、OBJ、GLB 等文件格式。
- 右键模型 → **3D 模型快速预览**，金属立方体图标用于辨认菜单项。
- 左键拖动旋转，中键或右键拖动平移，滚轮缩放，双击或按 `Home`/`R` 恢复视角，`Esc` 退出。
- C4D 文件继续显示场景保存时的图片；C4D 图片不含几何数据，因此窗口内只能缩放和平移。
- 通用模型读取和渲染完全独立运行，不需要安装 Blender、3ds Max、Maya 或其 SDK。
- 缩略图实际解析在受限工作进程中运行，20 秒超时、最大约 1 GiB 内存，避免损坏模型直接拖垮资源管理器。
- 每用户安装，不修改默认打开软件，也不注册全局快捷键。卸载会恢复安装前的缩略图关联。

## 直接支持的常用格式

主要格式：FBX、OBJ、3DS、3MF、COLLADA/DAE、DXF、glTF/GLB、STL、PLY、DirectX X、X3D、IFC、LightWave、Modo、OpenGEX、USD/USDZ 等。

安装器还为 Assimp 当前导入器包含的 AC/AC3D、ASE、B3D、BVH、COB、CSM、IQM、Irrlicht、M3D、MD2/MD3/MD5、MS3D、NDO、NFF、OFF、PMX、Quick3D、RAW、SCN、SIB、SMD、STEP、TER、UC、VTA、XGL、ZGL 等扩展名注册缩略图和右键预览。格式名被注册不代表任意版本、任意插件数据都能完整还原；窗口目前以静态网格和基础材质颜色为主。

以下图三格式在 0.2.0 中**没有注册为可预览**：Alembic `.abc`、Bullet `.bullet`、Forger `.fpk`、Illustrator `.ai`、Redshift Proxy `.rs`、OpenVDB `.vdb`、VRML `.wrl`。它们需要额外解析器或专有格式支持；普通 `.xml` 也没有全局注册，因为会错误接管大量非三维 XML 文件。

Blender `.blend` 不使用通用几何导入：Assimp 已弃用该格式支持。后续会采用 Blender 文件内置缩略图提取，避免假装能完整解析场景。

## 安装

1. 在 [Releases](https://github.com/Zaiel666/c4d-quick-preview/releases) 下载 `Model-QuickPreview-0.2.0-Windows-x64.zip`，不要下载 GitHub 自动生成的 Source code。
2. 完整解压 ZIP。
3. 双击 `Install.cmd`。安装器会自动卸载本项目旧的 0.1.0 注册，再安装新版。
4. 重新打开模型所在文件夹，切换为“大图标”或“超大图标”。
5. 右键模型选择“3D 模型快速预览”。Windows 11 可能需要进入“显示更多选项”。

安装包目前没有商业代码签名证书，Windows 可能显示来源提示。发布页提供 SHA-256 校验值。

详细说明见 [安装与使用](docs/安装与使用.md)，已完成的检查见 [测试记录](docs/测试记录.md)。

## C4D 说明

通用模型功能不依赖 C4D。若电脑装有带 `resource\libs\win64\win_thumbnail.dll` 的 Cinema 4D，安装器会自动启用 `.c4d` 场景图片缩略图；也可以在 PowerShell 中指定：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -Cinema4DPath "D:\Maxon Cinema 4D 2023"
```

本项目不打包、不上传 Maxon 的 DLL、应用程序或示例文件。

## 卸载

双击解压目录中的 `Uninstall.cmd`。卸载器逐项恢复安装前的当前用户注册表值；发现值后来被其他程序更改时会保留其他程序的设置。

Windows 可能仍缓存旧缩略图。卸载或升级后如果仍看到水滴图标或旧图，可以关闭并重新打开文件夹，必要时使用 Windows“磁盘清理”清除“缩略图”缓存。

## 构建与许可

GitHub Actions 使用 MSVC、CMake 和 vcpkg 构建 Assimp 版本的通用查看器及 Windows Shell 扩展。项目代码采用 MIT 许可；Assimp 及其运行库的许可见 [第三方说明](THIRD-PARTY-NOTICES.md)。

