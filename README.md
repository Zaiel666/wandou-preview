# 豌豆预览 · Wandou Preview

**Windows 10/11 x64 文件夹模型、视频与图片缩略图工具，当前版本 0.2.0。**

它让资源管理器直接显示 Blender、FBX、OBJ 等三维文件及 MP4、AVI 等视频封面缩略图。软件不添加右键菜单，不占用空格键，可以和 QuickLook 同时使用。

![豌豆预览金属立方体图标](assets/metal-cube-preview.png)

## 功能

- 资源管理器直接显示三维模型缩略图，左下角标注 FBX、OBJ、GLB 等文件格式。
- Blender `.blend` 文件直接读取保存时写入文件的内嵌场景预览，不启动 Blender；支持普通保存和 GZip 压缩保存。
- MP4、AVI、MOV、M4V、WMV、ASF、MPG/MPEG、MKV、WebM、MTS/M2TS 等视频提取约 10% 位置的一帧作为封面，不播放视频。
- C4D 文件显示场景保存时的图片，左下角显示 C4D；需要电脑已安装带缩略图组件的 Cinema 4D。
- Illustrator `.ai` 文件保存了 PDF 兼容数据时，可直接生成缩略图，不要求安装 Illustrator。
- Radiance `.hdr` 高动态范围图片使用内置解码器和自动曝光生成缩略图。
- 每用户安装，不需要管理员权限，不修改默认打开软件，不上传文件。
- 登录 Windows 后会启动一次轻量刷新进程，等待资源管理器就绪后通知它重新加载缩略图处理器，随后立即退出。
- 模型解析在独立工作进程中运行，有 20 秒和约 1 GiB 内存上限，避免异常文件拖住资源管理器。

## 支持格式

通用三维格式：`.blend`、`.fbx`、`.obj`、`.3ds`、`.3mf`、`.dae`、`.dxf`、`.gltf`、`.glb`、`.stl`、`.ply`、`.ifc`、`.lwo`、`.lws`、`.lxo`、`.stp`、`.usd`、`.usda`、`.usdc`、`.usdz`、`.x`、`.x3d`、`.x3db`。

专用图片预览：`.c4d`、`.ai`、`.hdr`。

视频格式：`.mp4`、`.avi`、`.mov`、`.m4v`、`.wmv`、`.asf`、`.mpg`、`.mpeg`、`.mpe`、`.m1v`、`.m2v`、`.ts`、`.mts`、`.m2ts`、`.mkv`、`.webm`、`.ogv`、`.flv`、`.f4v`、`.vob`、`.3gp`、`.3g2`。程序先使用 Windows 原生媒体组件，系统缺少 HEVC 等解码器时自动切换到随安装包提供的 FFmpeg 解码组件。

FBX、OBJ 等格式重点显示静态网格和基础材质颜色。动画、程序化材质、第三方渲染器节点及部分贴图不会完整还原。`.blend` 显示 Blender 保存到文件里的预览图；没有内嵌预览的文件不会生成错误画面。

## 安装

1. 在 [Releases](https://github.com/Zaiel666/wandou-preview/releases) 下载 `Wandou-Preview-0.2.0-Setup.exe`。
2. 双击安装程序，看到“安装完成”即可。
3. 重新打开文件夹，切换为“大图标”或“超大图标”。

从 Space Thumbnails 等旧缩略图工具升级后，如果仍看到旧的彩色水滴，请按 `F5` 或重新打开文件夹。新版安装器会同步刷新关联缓存，并在以后每次登录 Windows 时自动刷新一次。

旧的 C4D Quick Preview 安装会自动迁移。详细排错和卸载方法见 [安装与使用](docs/安装与使用.md)。

## C4D 说明

安装器会自动寻找 `resource\libs\win64\win_thumbnail.dll`。也可手动指定 Cinema 4D 目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -Cinema4DPath "D:\Maxon Cinema 4D 2023"
```

C4D 文件里通常只能独立取到保存时的二维场景图片。本项目不打包 Maxon 的 DLL 或示例文件。

## 卸载与许可

双击解压目录中的 `Uninstall.cmd`。卸载器会恢复安装前的缩略图关联；若关联后来被其他软件修改，会保留其他软件的新设置。

项目代码采用 MIT 许可。Assimp 及运行库许可见 [第三方说明](THIRD-PARTY-NOTICES.md)。
