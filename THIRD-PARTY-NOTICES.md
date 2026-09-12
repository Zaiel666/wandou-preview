# Third-party notices

The Windows package includes the Open Asset Import Library (Assimp) and the runtime libraries installed with its official vcpkg port. Assimp is licensed under the BSD 3-Clause license. Its source and license are available at <https://github.com/assimp/assimp>.

The package may also contain Assimp runtime dependencies copied from vcpkg. Their notices are included by the upstream packages and can be inspected in the corresponding vcpkg installed metadata used by the GitHub Actions build.

The Windows package includes FFmpeg libraries built by the official Microsoft vcpkg port for video decoding and frame conversion. The build uses FFmpeg's LGPL-compatible default feature set and does not enable the vcpkg `gpl` or `nonfree` features. FFmpeg source and license information are available at <https://ffmpeg.org/> and <https://github.com/FFmpeg/FFmpeg>.

AV1 video decoding is provided by dav1d through FFmpeg. dav1d is licensed under the BSD 2-Clause license; its source and license are available at <https://code.videolan.org/videolan/dav1d>.

Cinema 4D and Maxon components are not included. C4D thumbnail extraction uses a compatible `win_thumbnail.dll` already installed on the user's computer.
