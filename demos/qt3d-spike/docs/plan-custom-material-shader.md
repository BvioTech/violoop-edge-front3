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

## 结论（已通过读源码确认，不是猜测）：这条方向走不通

**这条方向被堵死了，原因和方向 B 是同一个根因。** 2026-11-02 读
`~/qt6.8-src/`（dev1 上，Qt 6.8.5 源码）确认了完整链路：

1. `src/runtimerender/res/effectlib/skinanim.glsllib`（Quick3D 内置的
   蒙皮库，`qt_getTexMatrix()`）**硬编码**用 `textureSize()`/
   `texelFetch()` 查骨骼矩阵纹理，**完全没有 ESSL100 fallback
   实现**——不管默认材质还是自定义材质，只要做蒙皮，最终都要用到同样
   的纹理查表手法（这是业界标准做法，骨骼数一多，uniform 数组会超限，
   没有别的通用替代方案）。
2. 关键的那一步在 `src/runtimerender/qssgrendershadercache.cpp` 的
   `initBakerForNonPersistentUse()`：

   ```cpp
   case QRhi::OpenGLES2: {
       QSurfaceFormat format = ...;
       format = h->context->format();   // 问题就是这一行
       ...
       if (format.majorVersion() >= 3)
           outputs.append({ QShader::GlslShader, QShaderVersion(300, GlslEs) });
       else
           outputs.append({ QShader::GlslShader, QShaderVersion(100, GlslEs) });
   ```

   这是**运行时**决定"这个 shader 要烤成哪个 GLSL 版本"的地方，而且
   **只烤一个版本，不是多个变体里挑**。它读的 `format.majorVersion()`
   就是 `findings.md`/`plan-patch-qt-source.md` 里记录的、卡在 `2.0`
   的那个 `QSurfaceFormat`。因为这个值是 2，Quick3D 直接决定"只生成
   ESSL100 版本"，把包含 `textureSize()` 的蒙皮代码烤成一份自称
   ESSL100、但内容非法的 shader，交给驱动编译器，驱动正确拒绝——这正是
   实测看到的报错原文。
3. **这条判断逻辑在 `QRhi::OpenGLES2` 这个 case 分支里统一生效，不区分
   默认材质还是自定义材质**——`CustomMaterial` 的 shader 最终也要经过
   同一个 `QSSGShaderCache`/`QShaderBaker` 流程，读的是同一个
   `h->context->format()`。手写 `#version 300 es` 的 `CustomMaterial`
   一样会被这个检查拦截、按 ESSL100 烤、一样编译失败——**版本选择不是
   按材质类型区分的，是按 RHI backend 全局决定的，Quick3D 没有给
   CustomMaterial 留"强制指定 GLSL 版本"的旁路**。

**推论（对后续工作很重要）**：反过来说，**如果方向 B 修好了（让
`ctx->format()` 正确报告 3.2），默认材质的蒙皮代码本身就是对的，根本
不需要写自定义材质**——`skinanim.glsllib` 的实现没问题，纯粹是被烤成
了错误的目标版本。所以方向 A 不是"次优方案"，而是"在方向 B 没修好之前
不可能成功，方向 B 修好之后又不需要"——两种情况下都不需要投入实现。

## 如果仍然要验证（可选，纯粹为了双重确认）

上面的结论是读源码得出的，有很高把握，但没有在真机上实测过
"CustomMaterial + texelFetch" 这个具体组合。如果想要实测级别的确认
（而不是代码分析级别），可以写一个极简的 `CustomMaterial`（不涉及蒙皮，
随便一个用 `texelFetch`/`textureSize` 的 fragment shader 即可）在 dev1
上跑，预期**会**报同样的 `textureSize is not supported in ESSL 100`。
如果真机结果和代码分析不一致（不报错），说明上面的源码分析漏看了什么，
需要重新定位——但目前没有计划投入这个验证，因为推论链路已经很直接。

## 原计划的实现步骤（已作废，保留存档）

下面几步是在确认"结论"一节之前写的原始计划，**不再推进**，留着是为了
如果以后发现上面的源码分析有误、这条方向被重新打开时，不用从头想一遍。

1. ~~跑通最小验证~~ → 已改为源码分析直接判定，见"结论"一节。
2. 读 glTF 蒙皮的标准做法，确认骨骼矩阵纹理的编码格式（`skinanim.glsllib`
   的 `qt_getTexMatrix()` 就是这套编码，4 个纹素一个 `mat4`，按
   `index*4` 定位——如果方向 B 修好后要抄这段实现，直接看这个文件，
   不用重新设计格式）。
3. 写 `CustomMaterial`，显式声明 GLSL 版本，手动实现骨骼矩阵纹理查表。
4. 用 `CesiumMan.glb` 测试，确认骨骼索引/权重数据的接入方式。
5. 真机验证渲染正确性、帧率、内存。

## 结论

- [x] 是否被同一个根因堵死：**是**，见上面"结论"一节，已通过读源码
  确认（`qssgrendershadercache.cpp` 的 `initBakerForNonPersistentUse()`
  按 `h->context->format().majorVersion()` 决定烤哪个 GLSL 版本，不分
  材质类型）。
- [ ] 如果以后想要实测双重确认，见"如果仍然要验证"一节，目前未执行。
- 这条方向目前**不再投入**，精力转向 `plan-patch-qt-source.md`——修好
  那边的版本协商问题后，默认材质的蒙皮会直接工作，不需要这份文档里
  计划的任何实现。
