# 开发环境（arch-pc VM）

记录这台机器上 aarch64 devkit 的安装方式，便于复现 / 迁移到 CI。

## 结论先行

- **devkitPro 官方源可以直连，但 Cloudflare 会按 User-Agent 拦截**：`curl`/`pacman`
  默认 UA 一律 403，带上浏览器 UA 就 200。所以不需要镜像，也不需要 docker。
- 安装的是 devkitPro 的 Linux 包，解包到 `/opt/devkitpro`（和官方 deb 布局一致）。
- **GCC 必须用 15.2.0**：仓库里最新的 16.1.0 会让 pinned 的 Atmosphere-libs
  (82f1553) 和 `ldnmitm_main.cpp` 报错（`-Werror=unused-but-set-variable`、
  `-Werror=c++23-extensions`、`operator new/delete` 缺 `noexcept`）。
  CI 当时用的是更旧的 GCC，所以 `:latest` 镜像现在也会踩到。

## 已安装

| 组件 | 版本 | 来源 |
| --- | --- | --- |
| devkita64-binutils | 2.46.0-1 | `packages/linux/x86_64/` |
| devkita64-gcc | **15.2.0-3** | `packages/linux/x86_64/` |
| devkita64-newlib | 4.6.0.20260123-4 | `packages/` |
| libnx | 4.12.0-1 | `packages/` |
| switch-tools (npdmtool/elf2nso/nxlink) | 1.13.1-1 | `packages/linux/x86_64/` |
| devkita64-rules + general-tools | 1.1.1 / 1.4.4 | 两者都要，否则缺 `devkitA64/base_rules` |
| switch-libjpeg-turbo | 2.1.2-2 | libstratosphere 的 capsrv 需要 `jpeglib.h` |

仓库地址：

- Linux 宿主主程序：`https://pkg.devkitpro.org/packages/linux/x86_64/`
- 与宿主无关的库：`https://pkg.devkitpro.org/packages/`

## 环境变量

`/etc/profile.d/devkitpro.sh`（已写入）：

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export PATH=$DEVKITA64/bin:/opt/devkitpro/tools/bin:$PATH
```

## 构建

```sh
cd ~/project/ldn_mitm
make -j$(nproc)
# 产物: out/sd/atmosphere/contents/4200000000000010/exefs.nsp
#       out/sd/switch/ldnmitm_config/ldnmitm_config.nro
#       out/sd/switch/.overlays/ldnmitm_config.ovl
```

## 踩过的坑

1. `make ATMOSPHERE_SETTINGS=...` / `make ATMOSPHERE_CXXFLAGS=...` **不能用**：
   命令行变量会整体覆盖，连带把 `-march=armv8-a+crc+crypto`、`-fno-rtti`、
   `-std=gnu++23` 一起冲掉，症状分别是 crypto 内部 intrinsic 报
   "target specific option mismatch"、以及链接时 `typeinfo for ServerManagerBase`
   未定义。
2. 如果想在新 GCC 上构建，正确做法是把 submodule 升到上游（当前 pin 落后 53 个
   commit），而不是压告警。
