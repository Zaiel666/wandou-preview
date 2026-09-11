# 豌豆预览 · Wandou Preview

**Windows 10/11 x64 文件夹模型缩略图与右键预览工具，当前版本 0.1.0。**

它让资源管理器直接显示 FBX、OBJ 等三维模型缩略图。右键模型选择 **3D 模型快速预览**，无需先打开 Blender、C4D、3ds Max 或 Maya。软件不占用空格键，可以和 QuickLook 同时使用。

![豌豆预览金属立方体图标](assets/metal-cube-preview.png)

## 功能

- 资源管理器直接显示三维模型缩略图，左下角标注 FBX、OBJ、GLB 等文件格式。
- 右键打开独立三维窗口：左键旋转，中键或右键平移，滚轮缩放，双击或按 `Home`/`R` 恢复视角。
- C4D 文件显示场景保存时的图片，左下角显示 C4D；需要电脑已安装带缩略图组件的 Cinema 4D。
- Illustrator `.ai` 文件保存了 PDF 兼容数据时，可直接生成缩略图和右键预览，不要求安装 Illustrator。
- Radiance `.hdr` 高动态范围图片使用内置解码器和自动曝光生成缩略图。
- 金属立方体图标显示在右键菜单和程序窗口中。
- 每用户安装，不需要管理员权限，不修改默认打开软件，不上传文件。
- 模型解析在独立工作进程中运行，有 20 秒和约 1 GiB 内存上限，避免异常文件拖住资源管理器。

## 支持格式

通用三维格式：`.fbx`、`.obj`、`.3ds`、`.3mf`、`.dae`、`.dxf`、`.gltf`、`.glb`、`.stl`、`.ply`、`.ifc`、`.lwo`、`.lws`、`.lxo`、`.stp`、`.usd`、`.usda`、`.usdc`、`.usdz`、`.x`、`.x3d`、`.x3db`。

专用图片预览：`.c4d`、`.ai`、`.hdr`。

通用模型窗口重点显示静态网格和基础材质颜色。动画、程序化材质、第三方渲染器节点及部分贴图不会完整还原。Blender `.blend` 不是稳定的独立交换格式；请从 Blender 导出 FBX、OBJ 或 GLB 后预览。

## 安装

1. 在 [Releases](https://github.com/Zaiel666/wandou-preview/releases) 下载唯一的 `Wandou-Preview-0.1.0-Windows-x64.zip`。
2. 完整解压 ZIP。
3. 双击 `Install.cmd`。
4. 重新打开模型文件夹，切换为“大图标”或“超大图标”。
5. 右键模型选择“3D 模型快速预览”。Windows 11 可能需要先点“显示更多选项”。

从 Space Thumbnails 等旧缩略图工具升级后，如果仍看到旧的彩色水滴，请按 `F5` 或重新打开文件夹。新版安装器会同步刷新关联缓存；右键打开一次模型也会刷新该文件的缩略图。

旧的 C4D Quick Preview 安装会自动迁移。详细排错和卸载方法见 [安装与使用](docs/安装与使用.md)。

## C4D 说明

安装器会自动寻找 `resource\libs\win64\win_thumbnail.dll`。也可手动指定 Cinema 4D 目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -Cinema4DPath "D:\Maxon Cinema 4D 2023"
```

C4D 文件里通常只能独立取到保存时的二维场景图片，因此 C4D 右键窗口支持缩放和平移，不能像 FBX/OBJ 一样旋转几何体。本项目不打包 Maxon 的 DLL 或示例文件。

## 卸载与许可

双击解压目录中的 `Uninstall.cmd`。卸载器会恢复安装前的缩略图关联；若关联后来被其他软件修改，会保留其他软件的新设置。

项目代码采用 MIT 许可。Assimp 及运行库许可见 [第三方说明](THIRD-PARTY-NOTICES.md)。
