# 交接：`violoop-qt-build` 发布的 .deb 在目标设备上完全起不来（glibc/ICU 不匹配）

> **已修复并验证（2026-11-03）。** `v6.8.5-violoop3` 改成在
> `debian:bookworm-slim` 容器里构建（CI diff 见下方"建议修法"一节，
> 实际采用的方案跟建议完全一致），dev1 上验证 `ldd` 干净、`qmake6`
> 正常、`qt3d_spike` 用这个包重新编译后离屏行为跟旧的手工编译 Qt
> 完全一致。dev1 上旧的手工编译 Qt（`/home/admin/qt6.8*`，约 21G）已经
> 删除腾空间。这份文档保留作为问题记录，不用再处理。

> 面向接手这个具体问题的人/agent。背景文档（两个真实 Qt bug 的调查过程、
> fork+构建的整体规划）在 `findings.md`、`plan-patch-qt-source.md`、
> `plan-qt-fork-and-build.md`，这里只讲这一个阻塞性问题，不用重新读完
> 那几份文档也能直接动手修。

## 结论先说

`BvioTech/violoop-qt-build` 仓库本身、里面的 `qtquicktimeline` 补丁、
`qtbase` 的 `-opengl es2` 配置，这些**都是对的**，已经在目标设备上用
手工编译的同一套源码验证过可以跑（骨骼动画正常渲染，见
`findings.md`"2026-11-03（第三轮）"）。**唯一的问题是 CI 的构建环境
选错了**，导致产出的 `.deb` 在目标设备上一个库都加载不起来。不需要
重新调查 Qt 本身的任何代码逻辑，只需要修 CI 的构建环境。

## 问题

Release：https://github.com/BvioTech/violoop-qt-build/releases/tag/v6.8.5-violoop1
产出：`violoop-qt6_6.8.5-violoop1_arm64.deb`（46211854 字节，sha256
`1fd4ed38d09034c9d986c86158ecde56262f0df9841eee7b08d0ee186d84ef3c`）。

这个包在目标设备 `dev1`（生产用 rk3576/Mali-G52 板子，**Debian 12
bookworm**，arm64）上装完之后，`qmake6` 本身都跑不起来：

```
/opt/violoop-qt6/bin/qmake6: error while loading shared libraries: libicui18n.so.74: cannot open shared object file: No such file or directory
```

`ldd` 看 `libQt6Core.so.6.8.5`：

```
libicui18n.so.74 => not found
libicuuc.so.74 => not found
```

`ldd` 看 `libQt6Gui.so.6.8.5`（以及 `libQt6Quick3D*`、`libQt6QuickTimeline*`
等几乎所有依赖 Gui/Qml 的库）：

```
version `GLIBC_2.38' not found (required by libQt6Gui.so.6.8.5)
version `GLIBCXX_3.4.32' not found (required by libQt6Quick3DRuntimeRender.so.6)
```

### 根因

`.github/workflows/build.yml` 里：

```yaml
jobs:
  build:
    runs-on: ubuntu-24.04-arm   # <-- 这里
```

CI 直接用 **Ubuntu 24.04** 的宿主环境编译（glibc 2.38/2.39、ICU 74），
但目标设备是 **Debian 12 bookworm**（glibc 2.36、ICU 72）。Qt 的库是
动态链接到系统 glibc/libstdc++/ICU 的，构建机的版本比目标设备新，产出
的二进度要求的符号版本目标设备根本没有——这是单向不兼容，**没法在
目标设备这边修**（给 bookworm 升级 glibc 等于升级整个系统，风险极高，
不应该在生产 kiosk 设备上做）。

**只能从构建侧修**：构建环境要跟目标设备（bookworm）对齐，而不是用
构建机宿主系统的版本。

### 次要问题（顺手一起修）

`scripts/package-deb.sh` 手写 `DEBIAN/control`，没有声明任何
`Depends:`（可以直接 `dpkg -I` 这个 `.deb` 确认，`grep -i depends` 没
输出）。即使修好了 glibc/ICU 对齐问题，包仍然是"装的时候不检查依赖，
运行时才发现缺东西"。建议在打包前跑一下 `dpkg-shlibdeps`（或等价
手段）让 `control` 文件里自动带上正确的 `Depends:`（比如 bookworm 上
实际的 `libicu72`、`libssl3` 等包名），这样环境不对的时候 `dpkg -i`
会直接拒装并报缺什么，而不是像现在这样装完看起来成功、实际完全跑
不起来。

## 建议修法

在同一个 `ubuntu-24.04-arm` runner 上，**用 Debian bookworm 容器跑整个
构建**（arm64 runner 自带 docker，不需要换 runner 类型）：

```yaml
- name: Build and package (inside Debian bookworm, matches target devices)
  run: |
    docker run --rm -v "$PWD:/work" -w /work \
      -e QT_PREFIX=/opt/violoop-qt6 \
      debian:bookworm-slim \
      bash -c '
        apt-get update &&
        apt-get install -y --no-install-recommends \
          build-essential cmake ninja-build git perl python3 pkg-config \
          libgles2-mesa-dev libegl1-mesa-dev \
          libfontconfig1-dev libfreetype-dev libssl-dev zlib1g-dev \
          libdrm-dev libgbm-dev libinput-dev libudev-dev \
          libwayland-dev wayland-protocols libxkbcommon-dev &&
        ./build-all.sh
      '
```

（上面是示意，不是最终 diff——`actions/cache` 那一步的缓存路径、
`gh release` 发布那几步不需要进容器，可以留在宿主上跑；容器内调用
`git`/网络要确认能正常访问 github.com。）

另一个可选方案是换成自带 bookworm 的 self-hosted arm64 runner，但
docker-in-GH-hosted-runner 改动小、不需要额外维护机器，优先选这个。

## 怎么验证修好了

1. CI 跑完后下载新 `.deb`，先别急着发 release，本地/CI 里跑：
   ```bash
   dpkg-deb -x <deb> /tmp/extract
   ldd /tmp/extract/opt/violoop-qt6/lib/libQt6Core.so.* | grep -i "not found"
   ldd /tmp/extract/opt/violoop-qt6/lib/libQt6Gui.so.* | grep -iE "GLIBC_2\.3[7-9]|GLIBC_2\.4"
   ```
   两条都应该没有输出（没有 `not found`，也没有要求比 2.36 更高的
   glibc 符号版本）。
2. 确认没问题后才打新 tag / 发 release（命名可以用
   `v6.8.5-violoop2`，说明是同一个 Qt 补丁版本但换了构建环境）。
3. 真机验证：
   ```bash
   scp <新.deb> dev1:/tmp/
   ssh dev1 'sudo dpkg -P violoop-qt6 2>/dev/null; sudo dpkg -i /tmp/<新.deb>'
   ssh dev1 '/opt/violoop-qt6/bin/qmake6 -v'   # 应该正常打印版本，不再报 icu 错误
   ```
   然后参考 `../scripts/launch-eglfs.sh` 的思路，把 `LD_LIBRARY_PATH`/
   `QML2_IMPORT_PATH`/`QT_PLUGIN_PATH` 指向 `/opt/violoop-qt6`，重新编译
   `qt3d_spike`，跑一次骨骼动画 demo，确认跟 `findings.md` 记录的、用
   手工编译 Qt 跑出来的结果一致（能看到模型、动画在动、没有 ESSL100/
   关键帧相关报错）。

## 现场状态（修复前，不要动的东西）

- dev1 上 `/opt/violoop-qt6` 现在装的是**这个坏掉的包**（版本
  `6.8.5-violoop1`）。确认新包能跑之后，先 `sudo dpkg -P violoop-qt6`
  卸掉旧的再装新的（同一个安装路径，不能并存两个版本）。
- dev1 上**手工编译的那套 Qt 还保留着**，在
  `/home/admin/qt6.8`（装好的，275M）+ `~/qt6.8-build`（build 目录，
  约 20G）+ `~/qt6.8-src`（源码，937M）。这是目前唯一已验证真正能跑
  的 Qt，**在新 `.deb` 真机验证通过之前不要删**——用户已经要求删掉它
  腾空间，但前提是新包先验证通过，顺序不能反。
- `qt3d-spike` 现在真机测试用的 launcher
  （`scripts/launch-eglfs.sh` / dev1 上的 `/tmp/qt3d-spike/launch-eglfs-68.sh`）
  都还指向手工编译那套 `/home/admin/qt6.8`，没有改。
- dev1 的 SSH（别名 `dev1`，实际 `192.168.1.50`）这次会话里出现过一次
  "No route to host"，重试后恢复——如果连不上，先重试，不代表设备
  真的坏了。
- 不要碰 `violoop-edge-kiosk.service`（生产 kiosk 服务）—— 真机显示
  测试的方法是临时 systemd drop-in 顶替 `ExecStart`，测完立刻换回去，
  具体步骤见 `../README.md`。改生产 kiosk 服务需要先取得明确授权。

## 完成后要做的收尾（不属于这个 bug 本身，但别漏）

- `plan-qt-fork-and-build.md`"执行结果"一节要更新：发布的新版本号、
  验证结果、什么时候真正删掉了 dev1 上手工编译的那套 Qt。
- `findings.md` 补一条记录：确认新 `.deb` 构建产出跟手工编译版本行为
  一致（或者不一致的话，记录差异）。
