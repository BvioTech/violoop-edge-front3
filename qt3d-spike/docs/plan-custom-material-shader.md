# 方向 A：自定义材质手写 ES3 shader，绕开 Quick3D 默认材质路径

> 背景和问题全貌见 `findings.md`（尤其"glTF 骨骼动画限制"和
> "2026-11-02（第二轮）"两节）。这里只摘录这条方向需要的最小背景，
> 方便独立接手。

## 问题一句话复述

Quick3D 给蒙皮网格生成的**默认材质** shader 用 `textureSize()`/
`texelFetch()` 查骨骼矩阵纹理，这两个函数需要 GLSL ES 3.00+
（ESSL300）。Qt 自己对 context 的版本记账卡在 `2.0`（即使驱动真实是
ES3.2），于是 Quick3D 的 shader 生成/选择逻辑挑了 ESSL100 变体，编译
失败。**非蒙皮渲染完全正常**，问题精确限定在"蒙皮默认材质 shader
生成"这一步。

## 这条方向的思路

不用 Quick3D 自动生成的默认材质，而是用 Quick3D 的
`CustomMaterial`（`QtQuick3D` 模块里支持手写 vertex/fragment shader
的材质类型），自己写一份显式声明版本、手动做骨骼矩阵纹理查表的
shader，绕开默认材质生成器里那条"按照（错误的）协商版本挑 shader
变体"的逻辑。

## 关键风险（第一步必须先验证，不要先投入写 shader）

`CustomMaterial` 的 shader 最终还是要经过 Qt 的 shader 编译管线
（`qsb`/shadertools 离线烘焙 + 运行时按 RHI backend 选变体），**不确定
这条管线对自定义材质 shader 的"版本选择"逻辑是否和默认材质共享同一处
协商 bug**。如果共享，那即使手写了 `#version 300 es` 的 shader，运行时
仍然可能被塞进 ESSL100 的编译路径，一样报错——这条方向就会被堵死，要
先转向方向 B（改 Qt 源码）。

**第一步**：写一个极简的 `CustomMaterial`（不涉及蒙皮，随便一个用
`texelFetch`/`textureSize` 的 fragment shader 即可），在 dev1 真机上跑，
看它是否报同样的 `textureSize is not supported in ESSL 100`。

- 如果**不报错**（绕开成功）→ 这条方向可行，继续往下做蒙皮版本。
- 如果**同样报错** → 说明版本协商 bug 影响的是 RHI/QPA 层面，材质类型
  （默认 vs 自定义）不影响，这条方向走不通，去同步方向 B 的人，把这个
  结论记回这份文档，然后两边一起转向方向 B。

## 可行路线下的实现步骤

1. 跑通上面的最小验证（见"关键风险"）。
2. 读一遍 glTF 蒙皮的标准做法，确认骨骼矩阵纹理的编码格式：每个骨骼一个
   `mat4`（4 个 `vec4` 纹素，或者用 RGBA32F 一行存一个矩阵），纹理宽度
   通常是 `4 * boneCount` 或类似布局——可以参考 Quick3D 源码
   `qtquick3d-everywhere-src-6.8.5/src/runtimerender/` 下默认材质生成
   蒙皮 shader 片段的实际写法（dev1 上 `~/qt6.8-src/
   qtquick3d-everywhere-src-6.8.5/` 已经有源码，不用重新下载），抄它的
   编码格式，保证和 Quick3D 其余管线（怎么把 `Model.skin`/骨骼变换数据
   传上来）兼容。
3. 写 `CustomMaterial`：
   - 显式声明 `#version 300 es`（或者用 Qt 的 shader 元语法声明需要的
     GLSL profile，具体写法以 Quick3D 文档 `CustomMaterial` 的
     `vertexShader`/`VARYING`/`INPUT` 约定为准）。
   - vertex shader 里手动实现：读骨骼索引/权重属性 → `texelFetch` 查骨骼
     矩阵纹理 → 按权重混合 → 应用到顶点位置/法线。
4. 用 `CesiumMan.glb` 测试：先确认骨骼索引/权重数据能从 `RuntimeLoader`
   导入的 model 上拿到（如果默认材质生成器内部拿这些数据的方式不是公开
   API，这里可能需要额外调研——不确定 `CustomMaterial` 能不能直接复用
   `RuntimeLoader` 导入的 skin 数据，还是需要自己解析 glTF 的
   `skins`/`joints`/`weights` 数组另起一套导入逻辑）。
5. 真机验证：渲染是否正确（动画是否播放、骨骼变形是否正确，不止是
   "不报错"）、帧率、短时内存（复用 `../scripts/sample_rss.sh`）。
6. 结果写回这份文档的"结论"一节（待填），和 `findings.md` 保持同步
   （在 `findings.md` 加一条指回这里）。

## 结论（待填）

- [ ] 最小验证：CustomMaterial 下 `textureSize`/`texelFetch` 是否可用
- [ ] 骨骼数据接入方式确认
- [ ] CesiumMan 蒙皮渲染是否跑通
- [ ] 帧率/内存验证
