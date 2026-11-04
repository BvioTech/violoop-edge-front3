# edge-front3

许可证：[GPL-3.0](LICENSE)。

设备触控屏 kiosk 前端，Qt/QML 实现。

## 目录结构

- `app/`：正式实现，Qt/QML 编写的 kiosk 应用。详见
  [`app/README.md`](app/README.md)。
- `qt3d-spike/`：基于 Qt Quick 3D 的角色动画渲染验证代码（含骨骼/蒙皮
  动画）。
- `ui-interface-spike/`：UI 模式覆盖与 REST/SSE 接口对接验证代码。

## 许可说明

角色动画依赖 `Qt Quick 3D` 模块，该模块开源版仅提供 GPL-3+ 授权（无 LGPL
选项），因此本仓库及最终依赖该模块的产品线按 GPL-3 开源发布。
