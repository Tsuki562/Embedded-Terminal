# ESP32P4 终端：预设内容清单与可实现路径

> 本文档对照《居家老人 AIoT 智能看护系统》作品报告（PDF），
> 列出 HomeCareHub 当前所有硬编码/预设内容，并逐条说明能否通过真实外部模块替换，以及替换方式。

---

## 一、当前所有预设内容清单

以下内容位于 `components/apps/homecare_hub/HomeCareHub.cpp` 的
`updateUi()` 函数内 `states[MODE_MAX]` 数组（约第 1184 行），
以及 `createUi()` 函数内的初始化值。

### 1.1 场景模式本身（DemoMode）

| 模式 | 名称 | 当前状态 |
|------|------|---------|
| MODE_NORMAL | 全屋安全 | 硬编码，每 6 秒自动轮播 |
| MODE_FALL | 异常复核 | 硬编码 |
| MODE_BATHROOM | 浴室看护 | 硬编码 |
| MODE_NIGHT | 夜间守护 | 硬编码 |

6 秒自动轮播逻辑在 `timerCb()`（约第 1639 行），纯演示用。

---

### 1.2 顶部状态栏

| 控件 | 当前预设值 | 数据来源 |
|------|-----------|---------|
| 时间 `_time_label` | `"08:24"` | 已接真实系统时钟（`std::time`），**非预设** |
| 日期 `_date_label` | `"5月26日"` | 已接真实系统时钟，**非预设** |
| 模式标签 `_mode_label` | `"全屋安全"` 等 | 按场景模式切换，**可被 MQTT 驱动** |
| 室外温度胶囊 `_top_weather_label` | `"室外 24°C"` | 预设；**天气 API 接入后自动覆盖** |
| 隐私模式 `_privacy_label` | `"在家模式"` | 按场景预设；可被 MQTT action 切换 |
| 系统副标题 `_status_label` | `"晴湾公寓 · 智能中控"` / 各模式状态描述 | **硬编码字符串** |

---

### 1.3 天气卡片（左侧面板）

| 控件 | 预设值 | 可实现程度 |
|------|--------|-----------|
| 城市名 `_weather_city_label` | 当前选中城市 | **已接真实城市选择** |
| 天气描述 `_weather_label` | `"多云"` / `"小雨"` 等 | **已接 Open-Meteo API，网络就绪后自动覆盖** |
| 主温度 `_weather_temp_label` | `"25°"` | **已接 Open-Meteo API** |
| 天气摘要 `_weather_summary_label` | `"客厅舒适 · 湿度 42%"` | 预设为室内描述；**网络就绪后被湿度值覆盖** |
| AQI `_aqi_label` | `"32"` | **硬编码，未接真实传感器** |
| CO2 `_co2_label` | `"620"` | **硬编码，未接真实传感器** |
| 噪声 `_noise_label` | `"28dB"` | **硬编码，未接真实传感器** |
| 室外温度 `_outdoor_label` | `"室外 24°C"` | **已接 Open-Meteo API** |
| 湿度 `_humidity_label` | `"湿度 58%"` | **已接 Open-Meteo API** |
| 空气质量 `_air_label` | `"优"` | **已接 Open-Meteo AQI，按等级着色** |

---

### 1.4 房间状态卡片（4 间房）

| 字段 | 预设示例 | 数据来源 |
|------|---------|---------|
| 房间名 | `"客厅"` / `"主卧"` / `"厨房"` / `"书房"` | **硬编码** |
| 活动描述 | `"主灯 68% · 空调 25°"` | **硬编码** |
| 状态标签（舒适/复核/看护）| `"舒适"` / `"复核"` | **硬编码** |
| 是否有人 | `true` / `false` | **硬编码** |
| CSI 强度值 | `31` / `18` / `14` / `20` | **硬编码** |
| 强调色 | 按模式切换 | **硬编码** |

---

### 1.5 事件/自动化日程（4 条）

| 字段 | 预设示例 | 数据来源 |
|------|---------|---------|
| 事件标题 | `"回家场景"` / `"跌倒复核"` / `"浴室久留"` 等 | **硬编码；MQTT event 可覆盖第一条** |
| 时间 | `"18:30"` / `"22:31"` 等 | **硬编码；MQTT event 可覆盖** |
| 事件描述 | `"玄关灯、热水、空调"` 等 | **硬编码；MQTT event 可覆盖** |

---

### 1.6 中央面板（场景/舒适度）

| 控件 | 预设值 | 数据来源 |
|------|--------|---------|
| 区域标题 `_hero_kicker_label` | `"LIVE · 客厅"` / 模式对应小车状态 | **部分被 MQTT attitude 覆盖** |
| 主标题 `_hero_room_label` | `"客厅主控"` / 模式名称 | **硬编码** |
| 描述文本 `_hero_text_label` | 各模式提示文本 | **部分被 MQTT attitude 覆盖** |
| 安全模式胶囊 `_security_chip_label` | `"在家模式"` 等 | **硬编码** |
| 舒适度温度 `_comfort_temp_label` | `"25°"` / `"23°"` / `"24°"` | **硬编码，按模式切换** |
| 环形温度指示 `_temp_arc` | 25 / 23 / 24 | **硬编码** |
| 灯光百分比 `_light_label` | `"68%"` / `"12%"` | **硬编码** |
| 功耗 `_power_label` | `"1.8kW"` / `"2.4kW"` | **硬编码** |
| 语音助手文本 `_assistant_text_label` | `"把客厅切到观影模式"` | **硬编码；呼叫后显示"正在呼叫家人"** |

---

### 1.7 巡检车面板（Page 2）

| 控件 | 预设值 | 数据来源 |
|------|--------|---------|
| 小车状态 `_car_status_label` | `"待命中"` / `"前往现场"` 等 | **硬编码；MQTT attitude 可覆盖** |
| 小车位置 `_car_position_label` | `"充电桩"` / `"走廊"` 等 | **硬编码；MQTT attitude 可覆盖** |
| 巡检目标 `_car_target_label` | `"客厅巡检点"` 等 | **硬编码；MQTT attitude 可覆盖** |
| 任务阶段 `_car_phase_label` | `"CSI 看护"` / `"CSI 复核"` 等 | **硬编码；MQTT attitude 可覆盖** |
| 障碍状态 `_obstacle_label` | `"障碍：通畅"` / `"已绕行"` | **硬编码；MQTT attitude 可覆盖** |
| 传感器状态 `_sensor_label` | `"传感器：CSI 在线"` | **硬编码；MQTT attitude 可覆盖** |
| 语音状态 `_voice_label` | `"语音：待命"` | **硬编码；MQTT attitude 可覆盖** |
| 小车电池 | 86% / 78% / 72% / 81% | **硬编码（目前未渲染到控件，仅存在 state 里）** |
| 路线进度 | 12% / 74% / 100% / 46% | **硬编码（目前未渲染到控件）** |

---

### 1.8 设备控制面板（Page 3）

| 控件 | 预设值 | 数据来源 |
|------|--------|---------|
| 客厅主灯状态 | `"亮度 68% · 暖白"` | **硬编码** |
| 中央空调状态 | `"制冷 · 低风速"` | **硬编码** |
| 南向窗帘状态 | `"开启 72%"` | **硬编码** |
| 客厅音响状态 | `"爵士电台 · 音量 32"` | **硬编码** |
| 设备开关 | 点击后只切换按钮颜色 | **仅 UI 状态，未发 MQTT 控制指令** |

---

### 1.9 安防 / 语音助手（Page 2 右侧）

| 内容 | 当前状态 |
|------|---------|
| 门厅安防摄像头 | 文字提示"请打开 Camera 应用"，无真实画面嵌入 |
| 安防模式按钮（在家/离家/夜间）| 只有样式变化，无真实逻辑 |
| 自动化日程（3 条）| 硬编码 18:30 / 21:15 / 23:40 |

---

## 二、可以通过 PDF 系统实现的部分

以下内容在 PDF 报告中有明确的实现路径，终端侧只需接收 MQTT 消息或调用已有接口即可替换对应预设值。

### 2.1 场景模式由 MQTT 驱动（已部分实现，可完全实现）

**PDF 描述：** 上位机或 CSI 节点通过 `smartcar/cmd` 发布 `state=abnormal` + `place` 触发异常，
小车状态机切换后向 `smartcar/system/status` 上报阶段，终端订阅后更新 UI。

**当前状态：** 已实现 MQTT 解析和模式切换入口。

**需要补充：**

1. 将 `smartcar/system/status` 的状态值（`cruise` / `abnormal_running` /
   `abnormal_ready` / `return_running`）映射到 `DemoMode`。
   当前 `fromMqttMode()` 只映射了 `normal / fall / bathroom / night`，
   需要扩展对应关系。

2. 删除或禁用 6 秒自动轮播定时器（`_timer`），改为仅由外部 MQTT 驱动。

**对应文件：**
- `components/apps/homecare_hub/HomeCareHub.cpp` — `timerCb()` 第 1629 行
- `components/apps/homecare_hub/HomeCareHub.cpp` — `fromMqttMode()` 第 1093 行
- `components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp` — `homecare_mqtt_parse_inbound()` 第 512 行

---

### 2.2 小车状态由 MQTT 驱动（已实现解析，UI 可扩展）

#### 2.2.1 姿态上报：`smartcar/attitude`

根据 `MQTT_PROTOCOL.md`，小车通过此主题上报当前动作，payload 字段因固件运行阶段不同而变化，
上位机应宽松解析：

```json
{
  "data": {
    "action": "stop"
  },
  "requestId": "nav_002"
}
```

`data.action` 可用于显示当前小车动作（`forward` / `backward` / `left` / `right` / `stop`）。

**当前状态：** 协议层已实现解析（`HomeCareMqttProtocol.cpp` 第 359 行），
收到后覆盖：`_car_status_label` / `_car_phase_label` / `_car_position_label` /
`_car_target_label` / `_obstacle_label` / `_sensor_label` / `_voice_label`。

**需要补充：**

- 解析 `data.action` 字段，将 `forward/backward/left/right/stop` 映射为中文状态文字。
- `_car_status_label` 目前写的是 `"SmartCar 在线"`，应替换为 `data.action` 对应的描述。

---

#### 2.2.2 系统状态上报：`smartcar/system/status`

这是更高层次的状态，反映整个系统状态机阶段，retain=true（新连接即收到最近状态）：

```json
{
  "state": "abnormal_running",
  "place": "bathroom",
  "phase": "begin",
  "ts": 12345678
}
```

| state | 中文含义 |
|-------|---------|
| `cruise` | 巡航中 |
| `abnormal_running` | 前往异常地点途中 |
| `abnormal_ready` | 已到达异常地点，等待处置 |
| `return_running` | 返航途中 |

典型状态流：

```
cruise → abnormal_running(begin) → abnormal_ready(arrived) → return_running(begin) → cruise(entered)
```

**需要补充：**

1. 将 `state` 字段映射到终端 `DemoMode`：
   - `cruise` → `MODE_NORMAL`
   - `abnormal_running` / `abnormal_ready`（place=bathroom）→ `MODE_BATHROOM`
   - `abnormal_running` / `abnormal_ready`（place=bedroom）→ `MODE_FALL`
   - `return_running` → 保持当前模式，更新状态文字

2. 根据 `state` 控制"返航"按钮的可点击状态：
   - 仅在 `abnormal_ready` 时允许点击返航。

3. 根据 `place` 字段更新 `_car_position_label` 和 `_car_target_label`。

**对应文件：**
- `components/apps/homecare_hub/HomeCareHub.cpp` — `updateUi()` 第 1355 行
- `components/apps/homecare_hub/HomeCareHub.cpp` — `fromMqttMode()` 第 1093 行
- `components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp` — `parse_smartcar_attitude_payload()` 第 359 行

---

### 2.3 事件列表由 MQTT event 驱动（已实现，可扩展）

**PDF 描述：** CSI 节点检测到异常后通过 `sensor/alert` 或 `smartcar/event` 发布事件，
终端订阅后在事件列表中插入。

**当前状态：** 已实现。收到 MQTT event 后覆盖事件列表第 0 条（最新事件）。
按 level 字段颜色区分：L3=珊瑚/L2=琥珀/L1=蓝/其他=绿。

**需要补充：**

1. 当前只覆盖第 0 条，可改为将事件推入队列，向下滚动展示历史记录
   （最多 4 条或可滚动）。

2. 可扩展接收 `smartcar/cmd` 里小车发出的 smartcar command 事件，
   目前 `HomeCareMqttProtocol.cpp` 已解析为事件插入，但终端没有
   订阅 `smartcar/cmd` 这个主题（通常是下发方向）。
   需确认 MQTT Bridge 订阅哪些主题。

**对应文件：**
- `components/apps/homecare_hub/HomeCareHub.cpp` — `applyMqttMessage()` 第 1117 行
- `components/apps/homecare_hub/HomeCareHub.cpp` — `updateUi()` 第 1421 行

---

### 2.4 天气数据（已实现，无需改动）

**当前状态：** 完整实现。

- 网络就绪后后台任务自动调用 Open-Meteo API。
- 返回温度、湿度、天气描述、AQI，覆盖预设值。
- revision 机制避免无意义重绘。
- 城市选择持久化到 NVS。

**AQI / CO2 / 噪声三个传感器值**目前是硬编码（32 / 620 / 28dB）。
PDF 中并未要求终端直接采集这些传感器，若需要真实值，
可在 MQTT 消息里携带并在 `applyMqttMessage()` 中扩展解析。

---

### 2.5 摄像头图传（已有框架，CameraViewer 应用负责）

**PDF 描述：**
- 摄像头发 JPEG 帧到 `cam/jpeg`
- 终端的 CameraViewer 接收并显示
- 支持 `cam/jpeg/control` 控制摄像头 `normal / silent / start_scan / off / lcd / mqtt`

**当前状态：**
- `CameraMqttReceiver` 已实现接收 `cam/jpeg` JPEG 帧、解码、获取快照。
  接口见 `components/apps/camera_viewer/CameraMqttReceiver.hpp`。
- `camera_mqtt_receiver_publish_mode()` 可发控制命令。
- HomeCareHub 里有一段被 `#if 0` 注释掉的 `refreshCameraPreview()`，
  曾经是在主屏显示摄像头画面的代码，现在改为引导用户去打开 Camera 应用。

**无需额外改动**，摄像头接入后 CameraViewer 可直接显示。
若需要在 HomeCareHub 主屏显示摄像头缩略图，将 `#if 0` 块恢复即可。

---

### 2.6 隐私模式 / 安防模式切换（部分实现）

**PDF 描述：** 语音模块或上位机通过 MQTT action 切换隐私模式；
摄像头可在隐私区域静默（`silent`），不采集图像。

**当前状态：**
- 终端点击"隐私切换"按钮会发 `HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE`，
  同时本地翻转 `_privacy_enabled` 标志并更新 UI。
- 但终端没有**接收**外部 MQTT 发来的隐私模式变更（只能自己发出）。

**需要补充：**

在 `homecare_mqtt_parse_inbound()` 里扩展解析 `privacy_toggle` action，
并在 `applyMqttMessage()` 里处理，使上位机也能远程切换终端的隐私显示状态。

---

### 2.7 小车控制按钮（需对齐真实 MQTT 协议）

#### 协议来源

根据 `MQTT_PROTOCOL.md`，所有小车控制均通过同一个主题：

```
smartcar/cmd
```

协议分为两类：**系统状态机控制** 和 **实时遥控**。

---

#### 系统状态机控制（3 个场景按钮）

终端 Page 2 面板上的场景按钮应发布以下 payload 到 `smartcar/cmd`：

**① 触发异常（前往指定地点）**

```json
{
  "cmd": "system",
  "state": "abnormal",
  "place": "bathroom",
  "requestId": "sys_abnormal_bathroom_001"
}
```

`place` 支持：
| place | 中文 | 小车动作序列 |
|-------|------|------------|
| `bathroom` | 卫生间 | 前进 10s → ���转 700ms → 前进 5s → 语音问询 → 摄像头扫描跟随 |
| `bedroom` | 卧室 | 前进 6s → 右转 700ms → 前进 4s → 语音问询 → 摄像头扫描跟随 |
| `kitchen` | 厨房 | 前进 8s → 右转 700ms → 前进 6s → 语音问询 → 摄像头扫描跟随 |

**② 返航（在 `abnormal_ready` 状态后才允许发）**

```json
{
  "cmd": "system",
  "state": "return_home",
  "place": "",
  "requestId": "sys_return_001"
}
```

行为：SmartCar 按上一次异常地点的反向路线返航，返回充电桩后自动通知 Voice/Camera 恢复正常，状态回到 `cruise`。

**③ 恢复巡航**

```json
{
  "cmd": "system",
  "state": "cruise",
  "place": "",
  "requestId": "sys_cruise_001"
}
```

---

#### 实时遥控（方向键/停止按钮）

```json
{
  "data": {
    "drive": "forward"
  },
  "requestId": "drv_1720000000000"
}
```

`drive` 支持的值：

| drive | 说明 |
|-------|------|
| `forward` | 短时前进 |
| `backward` | 短时后退 |
| `left` | 短时左转 |
| `right` | 短时右转 |
| `stop` | 停止当前遥控动作 |

---

#### 摄像头独立控制（可选，通常由小车自动联动）

正常流程下上位机无需直接控制摄像头；若需绕过小车单独操作，发布到 `cam/jpeg/control`：

```json
{"cmd": "system", "method": "start_scan", "place": "bathroom"}
{"cmd": "system", "method": "silent",     "place": "bathroom"}
{"cmd": "system", "method": "normal",     "place": ""}
{"cmd": "system", "method": "off",        "place": "bathroom"}
```

| method | 行为 |
|--------|------|
| `normal` | 恢复正常图传 |
| `silent` | 停止图传和跟随 |
| `start_scan` | MQTT 图传 + 水平扫描 + 识别后跟随 |
| `off` | 关闭摄像头联动 |

---

#### 当前实现状态

Page 2 巡检车面板已对齐真实协议：

1. "巡逻 / 回充 / 停止"均直接发布到绝对主题 `smartcar/cmd`，不会添加
   `homecare/hub/` 前缀。
2. "巡逻"发布 `state=cruise`，"回充"发布 `state=return_home`，
   "停止"发布 `data.drive=stop`。
3. 已增加"卫生间 / 卧室 / 厨房"地点按钮，发布对应的 `state=abnormal` 命令。
4. 每条命令使用设备毫秒时间戳生成 `sys_*` 或 `drv_*` 请求 ID。
5. 终端解析 `smartcar/system/status` 的状态机字段；仅收到 `abnormal_ready`
   后启用"回充"按钮，回调中也会再次校验状态。
6. 隐私切换和呼叫家人仍使用 HomeCare 自身的 `out/action` 主题，未与小车协议混用。

**对应文件：**
- `components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp` — `homecare_mqtt_format_action()` 第 588 行
- `components/apps/mqtt_bridge/HomeCareMqttBridge.cpp` — `homecare_mqtt_bridge_publish_action()`
- `components/apps/homecare_hub/HomeCareHub.cpp` — `actionEventCb()` 第 1529 行

---

### 2.8 设备开关（当前仅 UI，可扩展为 MQTT 控制）

**PDF 描述：** 上位机可通过 MQTT 控制设备；终端也应能发出设备控制命令。

**当前状态：** 设备开关（客厅主灯/空调/窗帘/音响）点击后只切换按钮颜色，
没有发出任何 MQTT 消息。

**实现路径：**

在 `actionEventCb()` 的 `action 10~13` 分支（第 1559 行）中，
切换 UI 的同时调用 `homecare_mqtt_bridge_publish_raw()` 向对应主题发布设备控制消息：

```cpp
// 示例：客厅主灯
homecare_mqtt_bridge_publish_raw("homecare/device/light", "{\"state\":\"on\"}", 1, 0);
```

具体主题格式需要和 SmartCarMode / SystemHost 约定。

---

## 三、依赖外部硬件、当前终端无法独立实现的部分

以下功能需要 PDF 中其他模块（小车固件、摄像头固件、语音模块、Python 上位机）
先接入，终端才能显示或联动，仅靠 ESP32P4 终端本身无法实现。

| 功能 | 依赖的外部模块 | 说明 |
|------|--------------|------|
| CSI-WiFi 异常检测 | ESP32S3 感知节点 | 终端只接收 MQTT event，不负责 CSI 采集 |
| 房间状态（有无人、CSI 强度）| CSI 感知节点 | 需节点通过 MQTT 上报每个房间的状态 |
| 跌倒 / 静止 / 离床 判断 | CSI 感知节点 + AI 推理 | 终端只显示结果，不做推断 |
| 卫生间久留计时 | CSI 感知节点 | 需节点采集并判断后上报 |
| 小车真实位置 / 路线进度 | SmartCarMode 小车固件 | 通过 `smartcar/attitude` 上报 |
| 摄像头实时画面（CameraViewer）| CameraMode 摄像头固件 | 通过 `cam/jpeg` 发 JPEG 帧 |
| 人脸识别 / 目标跟随结果 | CameraMode 摄像头固件 | 可通过 MQTT event 上报识别结果 |
| 语音询问 / 老人回应 | VoiceMode 语音模块 | 通过 UART 串口与小车联动 |
| 老人是否适 — 语音确认 | VoiceMode + 云端语音模型 | 确认后向终端上报 MQTT event |
| 系统状态机（巡航/异常/返航）| SmartCarMode 状态机 | 终端订阅 `smartcar/system/status` 显示 |
| 灯光 / 空调 / 窗帘 真实控制 | 智能家居设备 / 网关 | 需要对接具体设备协议 |
| AQI / CO2 / 噪声传感器数据 | 本地传感器节点 | 需通过 MQTT 上报传感器值 |
| 上位机 Web 页面 / MJPEG 视频流 | Python SystemHost | 独立于终端的 PC 端应用 |

---

## 四、实现优先级建议

### P0 — 不改代码就能用（接入外部模块即生效）

- MQTT attitude 驱动小车状态（终端已完整解析）
- MQTT event 驱动事件列表第一条（终端已完整实现）
- 摄像头 JPEG 图传在 CameraViewer 中显示（框架已完备）
- 天气数据（Wi-Fi 连接后自动拉取）

### P1 — 少量代码改动（1~2 天）

- 禁用 6 秒自动轮播定时器，改为仅 MQTT 驱动场景模式
- 将 `smartcar/system/status` 的状态映射到终端场景模式
- 在 Page 2 面板中显示小车电池百分比和路线进度
- 设备开关点击后发 MQTT 控制消息

### P2 — 中等改动（需协议约定，3~5 天）

- 房间状态卡片由 CSI 节点 MQTT 驱动（需定义每间房的上报主题和 payload 格式）
- 事件列表支持历史队列滚动展示
- 接收外部隐私模式切换命令
- 在 HomeCareHub 主屏嵌入摄像头缩略图（恢复 `#if 0` 块）

### P3 — 依赖外部系统完成后再做（1 周+）

- 语音助手文本由语音模块 MQTT 上报实时对话内容
- AQI / CO2 / 噪声由本地传感器节点上报
- 人脸识别 / 目标跟随状态由摄像头模块上报
- 灯光 / 空调 / 窗帘接入真实设备协议

---

## 五、关键文件索引

| 文件 | 作用 |
|------|------|
| `components/apps/homecare_hub/HomeCareHub.cpp` | 主仪表盘，所有预设数据和 UI 更新逻辑 |
| `components/apps/homecare_hub/HomeCareHub.hpp` | DemoMode 枚举、成员变量声明 |
| `components/apps/homecare_hub/HomeCareWeather.cpp` | 天气后台服务，Open-Meteo HTTP 请求 |
| `components/apps/homecare_hub/HomeCareWeatherCity.cpp` | 城市选择，NVS 持久化 |
| `components/apps/mqtt_bridge/HomeCareMqttBridge.cpp` | MQTT 连接、订阅、发布、消息队列 |
| `components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp` | MQTT payload 解析和格式化 |
| `components/apps/mqtt_bridge/HomeCareMqttProtocol.hpp` | 协议数据结构定义 |
| `components/apps/camera_viewer/CameraMqttReceiver.cpp` | 摄像头 JPEG 帧接收和解码 |
| `components/apps/setting/Setting.cpp` | Wi-Fi、NVS、城市选择，网络就绪后启动服务 |
| `main/main.cpp` | 入口，安装三个应用 |
| `MQTT_PROTOCOL.md` | 完整 MQTT 主题/payload 协议文档 |
