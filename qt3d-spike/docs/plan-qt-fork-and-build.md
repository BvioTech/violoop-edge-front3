# 方向 C（产品化）：fork Qt、打包、发到私有源

> 这份文档只是计划，**还没有执行任何一步**（没有 fork、没有建仓库、没
> 有发布）。背景和两个真实 bug 的完整调查过程见 `findings.md` 和
> `plan-patch-qt-source.md`——这里只接着往下规划"怎么把 dev1 上手动跑通
> 的那套 Qt，变成能长期维护、能装到别的设备上的东西"。

## 背景：到底需要改什么

dev1 上跑通骨骼动画，实际依赖两类改动：

1. **`qtbase`：不需要改任何一行源码**，只是编译参数不对——之前自动
   `configure` 探测成了桌面 OpenGL（`FEATURE_opengl_desktop=ON`），用
   `-opengl es2` 重新 `configure` 就修好了。这是**构建配置问题，不是
   代码补丁**。
2. **`qtquicktimeline`：需要一个真实的源码改动**——
   `QQuickKeyframeGroupPrivate::loadKeyframes()` 里一个 off-by-one 的
   循环边界，细节和具体代码见 `plan-patch-qt-source.md`"2026-11-03
   （第三轮）"。
3. `qtshadertools`/`qtdeclarative`/`qtwayland`/`qtquick3d`：都不需要
   改代码，只是要跟着 `qtbase` 用对的参数重新编译、按正确顺序安装。

**结论：6 个模块里，只有 `qtquicktimeline` 真的需要 fork 并打补丁。**
其余 5 个（包括 `qtbase`）直接引用官方 `v6.8.5` 标签源码构建即可，不需
要单独 fork（取舍见下方"决定"一节——已经选定这个轻量方案，不是还在摇摆）。

## 决定：方案 A（轻量）

只 fork `qtquicktimeline`。其余模块直接从官方 tag 拉源码构建，不建副本
仓库。相比"6 个模块全 fork"的方案，维护负担小很多，代价是 5 个模块的
构建依赖官方仓库长期可访问——如果以后出于 GPL"对应源码"合规的考虑需要
把这 5 个也镶一份到自己名下，是后续可以再补的独立任务，不影响现在这步。

## 第一步：fork `qtquicktimeline`，打补丁

1. Fork `https://github.com/qt/qtquicktimeline` 到
   `BvioTech/qtquicktimeline`（命名跟现有的 vendor fork 保持一致，参考
   `BvioTech/mpp`、`BvioTech/librga`、`BvioTech/rknn-toolkit2`——不加
   `violoop-` 前缀，就用原名）。
2. 以官方标签 `v6.8.5` 为基点建一个分支（例如
   `violoop/v6.8.5`），在上面提交那个 off-by-one 修复：

   ```cpp
   // src/timeline/qquickkeyframe.cpp,
   // QQuickKeyframeGroupPrivate::loadKeyframes()
   // 原来：
   for (qsizetype i = 0; i < arraySize - frameSize; i += frameSize) { ... }
   // 改成：
   for (qsizetype i = 0; i + frameSize <= arraySize; i += frameSize) { ... }
   ```

   commit message 要把根因和复现场景写清楚（off-by-one 导致每个通道
   漏掉最后一个关键帧，单关键帧通道直接变成空列表），可以直接照抄
   `plan-patch-qt-source.md` 里已经写好的那段分析，不用重新想一遍。
3. 打一个版本 tag，比如 `v6.8.5-violoop1`（6.8.5 基线 + 我们自己的第 1
   个补丁版本号），作为构建脚本要钉死引用的那个确切 ref。
4. （可选，不阻塞发布，但建议做）把这个修复提给 Qt 官方
   Gerrit（`codereview.qt-project.org`），作为一个真实存在的上游 bug
   报告/补丁。这是独立任务，不影响我们自己这边先把 fork 和构建跑起来。

## 第二步：新建 `violoop-qt-build` 仓库

放构建脚本，照搬 dev1 上已经手工验证过的流程（模块顺序、
`configure`/`qt-configure-module` 参数——见
`plan-patch-qt-source.md`"已有环境"一节和 `findings.md`"2026-11-02
/2026-11-03"几轮记录），不用重新摸索：

- 拉源码：
  - `qtbase`、`qtshadertools`、`qtdeclarative`、`qtwayland`、
    `qtquick3d`：官方 `v6.8.5` 标签（`https://github.com/qt/<module>`）。
  - `qtquicktimeline`：我们自己 fork 的 `v6.8.5-violoop1` 标签。
- 构建顺序：`qtbase` → `qtshadertools` → `qtdeclarative` →
  `qtwayland`/`qtquicktimeline` → `qtquick3d`（和 dev1 上验证过的顺序
  一致）。
- `qtbase` 的关键参数：`-opengl es2`（而不是让它自动探测成桌面
  OpenGL——这是整个问题的根因，构建脚本必须显式写死这个参数，不能依赖
  自动探测，否则换一台装了 Mesa 开发包的构建机就会复现同一个坑）。
- 安装 prefix：建议用独立路径（例如 `/opt/violoop-qt6`），不要覆盖
  Debian 系统自带的 `qt6-*`（6.4.2）——避免跟系统其它可能依赖系统 Qt
  的东西冲突，也方便将来并存多个版本/回滚。
- 打包：产出 `.deb`（给现有的私有 APT 流程用；具体接到哪个发布仓库/
  流程不在这份文档的范围内，留给下一步单独讨论）。
- CI：构建脚本写完之后，可以考虑接到 GitHub Actions 里自动触发（比如
  `qtquicktimeline` 打新 tag 时自动重新构建整套），这部分也是独立任务，
  不阻塞先把手动构建脚本跑通。

## 明确不在这份计划范围内的事

- 发布到哪个私有 APT 仓库/用什么工具（reprepro/aptly 等）发布——用户
  已经说了先不管，等 Qt 这边的 fork+构建跑顺了再接。
- 6 个模块是否要全部镶一份到 `BvioTech` 下（GPL 合规的更保守方案）——
  已经决定先不做，见上面"决定"一节，有需要再单独立项。
- 真正的 `edge-front3` 产品实现（选什么角色资产、怎么跟 `edge-front2`
  共存/替换）——这是三块里的第二块，还没开始讨论。

## 结论 / 待办

- [ ] fork `qt/qtquicktimeline` 到 `BvioTech/qtquicktimeline`
- [ ] 在 `v6.8.5` 基础上提交 off-by-one 修复，打 `v6.8.5-violoop1` 标签
- [ ] （可选）把修复提给 Qt 官方 Gerrit
- [ ] 新建 `violoop-qt-build` 仓库，写构建脚本（固定 `-opengl es2`，
  独立安装 prefix，产出 `.deb`）
- [ ] 发布目标/流程——留给下一步单独讨论
