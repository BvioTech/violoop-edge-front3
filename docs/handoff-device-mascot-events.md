# 交接：主页章鱼需要 violoop-edge 提供的任务状态事件

> 写给处理 `device` 仓库（violoop-edge）的人或 agent。2026-11-16。
> 提需求的一方是 `edge-front3`（设备屏幕上的 kiosk 应用）。

## 背景

kiosk 主页是一只 3D 章鱼，播哪段动画由 `src/MascotDirector` 决定。它需要
知道设备上的 AI 任务处于什么状态：

| 状态 | 章鱼的表现 | kiosk 这边的输入 |
|---|---|---|
| 工作中 | 敲电脑 / 看书 / 代码流三选一，循环 | `set(Working, 目标 id)` / `clear(Working)` |
| 完成 | 撒花 5.4 秒，只播一次 | `cue(Done)` |
| 异常 | 橘红双脉冲，循环到解除 | `set(Error)` / `clear(Error)` |

离线（`GET /status/connectivity`）和等待审批（`GET /sign/authorize-status`
加 `notification` 事件）已经接好，不在本文范围内。

kiosk 只连本机 `127.0.0.1:9090`：`GET /events`（SSE）和各 REST 端点。它
不是 `PeerRegistry` peer，收不到 `session.stream`。

## 现在缺什么

`/events` 上和任务有关的只有 `plan.*`（`core/tools/builtin/planning.js`）：

- 只有 agent 建了执行计划的任务才有；不建计划的任务没有任何事件。
- 计划被放弃或任务失败时没有事件，kiosk 会一直停在"工作中"。
- `plan.updated` 的 `status: failed` 是单步失败，不等于任务失败。
- 没有快照端点。`/events` 不重放、会丢帧（`local-http-events.md` §2），
  kiosk 重连或启动后无法知道当前是否有任务在跑。

`device` 内部已经有一条接近的信号：EventBus 的
`session:state {id, state: 'busy'|'idle'}`（`core/rpc/handlers/chat.js`
发，`core/services/led_state.js` 用它切灯带的忙闲）。它没有转到 `/events`，
而且只有忙闲，没有"目标"和"成败"。

## 需求

设计依据是设计师的交互文档（`vio-interaction.design.json` 的
`goal_work_scheduler`，要点已抄在下面，不需要去找原文件）。

### 1. 目标

一个**目标**是用户在聊天里提出的一次请求，从用户发出消息开始，到 agent
把这件事做完（或失败、被取消）为止。中间的思考、工具调用、重试、澄清、
等待审批都属于同一个目标。

需要一个稳定的目标 id，满足：

- 同一目标从头到尾不变，包括等待审批前后、异常重试前后。
- 不同目标 id 不同（kiosk 靠它给下一个目标换一段工作动画）。
- 来自后台任务、定时任务、语音派单的目标同样要有。

id 怎么构成由 device 决定，kiosk 只把它当不透明字符串比较相等。设计文档
的建议是 agent、会话、根用户目标三者合起来。

### 2. 工作中

目标开始执行时通知一次，目标结束（完成、失败、取消都算）时通知一次。
同一目标内部的阶段变化不需要通知，也**不应该**表现为"结束又开始"：
等待审批、等待用户回话、工具调用之间的空隙都仍然算工作中。

多个会话同时有目标在跑时，kiosk 只需要知道"至少有一个在跑"和其中一个
的 id；选哪个由 device 定（建议：最近开始的那个），但在它结束前不要
来回切换。

### 3. 完成

只在**用户的目标真正完成**时发，每个目标至多一次：

- 一轮回复结束、会话变 idle、裸 Stop 都不等于完成。
- 被取消、被用户打断、失败的目标不发完成。
- 同一个完成不能重发（kiosk 每收到一次就撒一次花）；重启、重连、快照
  对账都不能补发历史上已经完成的目标。

如果 device 无法区分"目标完成"和"一轮回复结束"，请在回复里说明能做到
哪一步，由我们和设计师商量降级方案（例如只对有计划的任务庆祝，沿用
`plan.completed`）。

### 4. 异常

目标执行失败、且需要用户知道时进入异常；之后有新目标开始、同一目标
重试、或用户处理掉了，就解除。需要明确的开始和解除，不能只有开始。

不算异常的：单个工具调用失败后 agent 自己恢复了；断网（那是离线，
kiosk 自己判断）；用户主动取消。

### 5. 快照

一个 REST 端点，返回当前时刻的状态，kiosk 在启动、SSE 重连、以及
工作中每隔几秒各查一次，以它为准，事件只当作"该去查了"的提示。至少
包含：当前是否有目标在跑及其 id、当前是否处于异常。

"完成"是一次性的，不进快照（否则重连会补撒花）。

## 接口形状的建议

以下只是建议，device 可以按自己的约定改，改完告诉我们最终形状即可。

事件（沿用 `/events` 的扁平信封）：

```jsonc
{ "type": "goal:state", "v": 1, "time": 1733400000,
  "goalId": "…",                       // 不透明字符串
  "state": "working|done|failed|cancelled",
  "sessionId": "…" }                   // 可选，调试用
```

- `working`：目标开始，或从异常/等待中恢复。重复发同一 `goalId` 无害。
- `done` / `failed` / `cancelled`：目标结束，各至多一次。
- 异常的解除：下一条 `working`（同一或新的 `goalId`），或一条专门的
  解除事件，二者选一并写明。

快照：

```jsonc
// GET /status/goal
{ "active": { "goalId": "…", "state": "working|failed", "sessionId": "…" } | null }
```

## 验收

在设备上 `curl -N http://127.0.0.1:9090/events` 观察：

| 场景 | 预期 |
|---|---|
| 从 PC 或手机发一个不需要建计划的任务 | 一条 `working`，结束时一条 `done` |
| 任务中途触发审批，批准后继续 | 期间没有新的 `working`/结束事件，`goalId` 不变 |
| 同一会话紧接着发第二个任务 | 新的 `goalId` |
| 任务进行中用户取消 | `cancelled`，没有 `done` |
| 任务失败 | `failed`；快照 `state: failed`；发新任务后解除 |
| 普通闲聊一问一答 | 请说明算不算目标；设计师的意思是只有"做事"才算 |
| 任务进行中重启 kiosk（`systemctl restart violoop-edge-kiosk-qt`） | 快照能查到在跑的目标 |
| 任务完成后重启 violoop-edge | 不补发 `done` |

另请更新 `docs/protocols/local-http-events.md` 和 `local-http.md`。

## kiosk 这边之后要做的

拿到最终接口后，在 `src/MascotInputs` 里加一个事件源：`working` →
`set(Working, goalId)`，结束 → `clear(Working)`，`done` 另加
`cue(Done)`，`failed` → `set(Error)`，解除 → `clear(Error)`；快照用于
对账。已有的离线和等待审批两个源是同一个写法，可以参照。
