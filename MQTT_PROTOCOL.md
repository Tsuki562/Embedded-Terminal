# 上位机 MQTT 通信协议文档

本文档用于第三方上位机接入本项目的 MQTT 系统链路。上位机只需要连接同一 MQTT Broker，并按本文档发布/订阅主题，即可控制 SmartCar、Camera，并接收系统状态、姿态与图传数据。

## 1. 连接参数

| 项目 | 值 | 说明 |
| --- | --- | --- |
| Broker URI | `mqtts://x3a5a71f.ala.cn-hangzhou.emqxsl.cn:8883` | TLS MQTT |
| Host | `x3a5a71f.ala.cn-hangzhou.emqxsl.cn` | 从 URI 解析 |
| Port | `8883` | MQTT over TLS |
| Username | `spic` | 上位机默认账号 |
| Password | `lhr20041005` | 与当前上位机一致 |
| TLS CA | `SmartCarMode/host_pc/certs/emqxsl-ca.pem` 或 `CameraMode/main/mqtt_ca.pem` | 二选一，需校验证书 |
| Client ID | 建议自定义且唯一 | 例如 `third_host_<时间戳>` |

> 注意：SmartCar 固件使用自己的 client-id 和账号配置。第三方上位机不要复用设备端 client-id，避免 MQTT 会话互踢。

## 2. 主题总览

| 主题 | 方向 | QoS 建议 | Retain | 说明 |
| --- | --- | --- | --- | --- |
| `smartcar/cmd` | 上位机 -> SmartCar | 1 | false | 系统状态机控制、小车遥控 |
| `smartcar/system/status` | SmartCar -> 上位机 | 1 | true | 系统状态机状态上报 |
| `smartcar/attitude` | SmartCar -> 上位机 | 0 | false | 小车姿态/遥控状态等上报 |
| `cam/jpeg/control` | 上位机/SmartCar -> Camera | 1 | false | 摄像头静默、扫描跟随、关闭、恢复 |
| `cam/jpeg/status` | Camera -> 上位机 | 0 | true | 摄像头运行模式状态 |
| `cam/jpeg` | Camera -> 上位机 | 0 | false | JPEG 二进制图传帧 |

推荐第三方上位机最少订阅：

```text
smartcar/system/status
smartcar/attitude
cam/jpeg/status
cam/jpeg
```

推荐第三方上位机最少发布：

```text
smartcar/cmd
```

如果需要绕过 SmartCar 直接控制摄像头，也可以发布：

```text
cam/jpeg/control
```

## 3. 系统状态机控制

### 3.1 发布主题

```text
smartcar/cmd
```

### 3.2 标准 JSON 格式

```json
{
  "cmd": "system",
  "state": "abnormal",
  "place": "bathroom",
  "requestId": "sys_1720000000000"
}
```

字段说明：

| 字段 | 类型 | 必填 | 取值 | 说明 |
| --- | --- | --- | --- | --- |
| `cmd` | string | 建议 | `system` | 表示系统状态机命令 |
| `state` | string | 是 | `cruise` / `abnormal` / `return_home` | 目标系统状态 |
| `place` | string | 异常模式必填 | `bedroom` / `bathroom` / `kitchen` | 异常地点 |
| `requestId` | string | 建议 | 自定义唯一字符串 | 便于上位机侧追踪请求 |

### 3.3 巡航模式

发布：

```json
{
  "cmd": "system",
  "state": "cruise",
  "place": "",
  "requestId": "sys_cruise_001"
}
```

行为：

- SmartCar 进入巡航状态，恢复原有自主逻辑。
- SmartCar 通过串口通知 Voice：`system_normal`。
- SmartCar 通过 MQTT 通知 Camera：`normal`。
- SmartCar 上报 `smartcar/system/status`，状态为 `cruise`。

### 3.4 异常模式

发布：

```json
{
  "cmd": "system",
  "state": "abnormal",
  "place": "bathroom",
  "requestId": "sys_abnormal_bathroom_001"
}
```

支持地点：

| place | 中文含义 |
| --- | --- |
| `bathroom` | 卫生间 |
| `bedroom` | 卧室 |
| `kitchen` | 厨房 |

行为：

- SmartCar 先通知 Voice 静默：串口方法 `system_silent`。
- SmartCar 再通知 Camera 静默：MQTT 方法 `silent`。
- SmartCar 按地点执行硬编码路线。
- 到达后 SmartCar 通知 Voice 开始问询：串口方法 `system_inquiry`。
- 到达后 SmartCar 通知 Camera 开始扫描跟随：MQTT 方法 `start_scan`。
- 路线完成后 SmartCar 上报状态 `abnormal_ready`，等待返航命令。

异常路线：

| 地点 | 动作序列 |
| --- | --- |
| `bathroom` | 语音静默 -> 摄像头静默 -> 前进 10s -> 左转约 700ms -> 前进 5s -> 语音问询 -> 摄像头扫描跟随 |
| `bedroom` | 语音静默 -> 摄像头静默 -> 前进 6s -> 右转约 700ms -> 前进 4s -> 语音问询 -> 摄像头扫描跟随 |
| `kitchen` | 语音静默 -> 摄像头静默 -> 前进 8s -> 右转约 700ms -> 前进 6s -> 语音问询 -> 摄像头扫描跟随 |

兼容文本格式：

SmartCar 当前也兼容部分文本/中文指令，例如：

```text
巡航、卫生间
abnormal bathroom
emergency kitchen
```

第三方上位机推荐始终使用标准 JSON，避免中文编码差异导致解析失败。

### 3.5 返航模式

发布：

```json
{
  "cmd": "system",
  "state": "return_home",
  "place": "",
  "requestId": "sys_return_001"
}
```

行为：

- SmartCar 使用上一次异常模式的地点作为返航地点。
- 如果没有历史地点，默认按 `bathroom` 路线返航。
- SmartCar 通知 Voice 关闭/停止操作：串口方法 `system_off`。
- SmartCar 通知 Camera 关闭操作：MQTT 方法 `off`。
- SmartCar 执行对应地点的反向路线。
- 回到起点后 SmartCar 通知 Voice `system_normal`，通知 Camera `normal`，并回到 `cruise`。

返航路线：

| 地点 | 动作序列 |
| --- | --- |
| `bathroom` | 语音关闭 -> 摄像头关闭 -> 掉头约 1400ms -> 前进 5s -> 右转约 700ms -> 前进 10s -> 语音正常 -> 摄像头正常 |
| `bedroom` | 语音关闭 -> 摄像头关闭 -> 掉头约 1400ms -> 前进 4s -> 左转约 700ms -> 前进 6s -> 语音正常 -> 摄像头正常 |
| `kitchen` | 语音关闭 -> 摄像头关闭 -> 掉头约 1400ms -> 前进 6s -> 左转约 700ms -> 前进 8s -> 语音正常 -> 摄像头正常 |

## 4. 系统状态上报

### 4.1 订阅主题

```text
smartcar/system/status
```

### 4.2 Payload 格式

```json
{
  "state": "abnormal_running",
  "place": "bathroom",
  "phase": "begin",
  "ts": 12345678
}
```

字段说明：

| 字段 | 类型 | 取值 | 说明 |
| --- | --- | --- | --- |
| `state` | string | `cruise` / `abnormal_running` / `abnormal_ready` / `return_running` | SmartCar 当前系统状态 |
| `place` | string | `bedroom` / `bathroom` / `kitchen` / 空字符串 | 当前状态关联地点 |
| `phase` | string | `entered` / `begin` / `arrived` 等 | 状态阶段 |
| `ts` | number | 毫秒 | SmartCar 启动后的毫秒时间戳 |

典型状态流：

```text
cruise
  -> abnormal_running, phase=begin
  -> abnormal_ready,   phase=arrived
  -> return_running,   phase=begin
  -> cruise,           phase=entered
```

第三方上位机建议以 `smartcar/system/status` 为准刷新 UI 状态，不要仅依赖自己发布命令后的本地状态。

## 5. 摄像头控制协议

### 5.1 发布主题

```text
cam/jpeg/control
```

### 5.2 Payload 格式

```json
{
  "cmd": "system",
  "method": "start_scan",
  "place": "bathroom"
}
```

字段说明：

| 字段 | 类型 | 必填 | 取值 | 说明 |
| --- | --- | --- | --- | --- |
| `cmd` | string | 建议 | `system` | 表示系统联动控制 |
| `method` | string | 是 | `normal` / `silent` / `start_scan` / `off` | 摄像头动作 |
| `place` | string | 否 | `bedroom` / `bathroom` / `kitchen` | 关联地点，可为空 |

方法说明：

| method | 行为 |
| --- | --- |
| `normal` | 恢复正常模式，允许 JPEG 图传 |
| `silent` | 静默摄像头，停止 JPEG 发布和跟随 |
| `start_scan` | 切换到 MQTT 图传模式，舵机水平缓慢扫描，识别到目标后进入跟随 |
| `off` | 关闭摄像头联动操作，停止图传和跟随 |

示例：

```json
{"cmd":"system","method":"silent","place":"bathroom"}
```

```json
{"cmd":"system","method":"start_scan","place":"bathroom"}
```

```json
{"cmd":"system","method":"off","place":"bathroom"}
```

```json
{"cmd":"system","method":"normal","place":""}
```

正常系统链路下，第三方上位机不需要直接控制 Camera；只需要向 `smartcar/cmd` 发布系统状态命令，由 SmartCar 自动联动 Camera。

## 6. 摄像头状态与图传

### 6.1 摄像头状态

订阅主题：

```text
cam/jpeg/status
```

Payload 示例：

```json
{"mode":"mqtt"}
```

字段说明：

| 字段 | 类型 | 取值 | 说明 |
| --- | --- | --- | --- |
| `mode` | string | `mqtt` / `lcd` | 摄像头当前运行输出模式 |

### 6.2 JPEG 图传

订阅主题：

```text
cam/jpeg
```

Payload：

- 二进制 JPEG 数据。
- 判断帧头可检查前两个字节：`0xFF 0xD8`。
- 建议上位机按 MJPEG 流方式展示，不要把该主题当作 UTF-8 文本解析。

## 7. 小车遥控协议

小车遥控与系统状态机共用主题：

```text
smartcar/cmd
```

推荐 JSON 格式：

```json
{
  "data": {
    "drive": "forward"
  },
  "requestId": "drv_1720000000000"
}
```

支持动作：

| drive | 说明 |
| --- | --- |
| `forward` | 短时前进 |
| `backward` | 短时后退 |
| `left` | 短时左转 |
| `right` | 短时右转 |
| `stop` | 停止当前短时遥控动作 |

兼容字段：

SmartCar 也兼容 `move` / `motion` / `cmd` 等字段中的 `forward`、`left`、`right`、`stop` 等文本。第三方上位机推荐使用 `data.drive`。

## 8. 小车姿态上报

订阅主题：

```text
smartcar/attitude
```

Payload 为 JSON。不同固件运行阶段字段可能不同，上位机应按宽松方式解析。

示例：

```json
{
  "data": {
    "action": "stop"
  },
  "requestId": "nav_002"
}
```

建议：

- 如果字段存在 `data.action`，可用于显示当前小车动作。
- 其他姿态字段保持透传展示，不应因缺字段报错。

## 9. 推荐上位机接入流程

1. 使用唯一 Client ID 连接 MQTT Broker。
2. 启用 TLS，并加载 CA 证书。
3. 订阅：
   - `smartcar/system/status`
   - `smartcar/attitude`
   - `cam/jpeg/status`
   - `cam/jpeg`
4. UI 初始化为 `cruise`，随后以 `smartcar/system/status` 的实际上报覆盖。
5. 用户点击异常地点时，向 `smartcar/cmd` 发布 `state=abnormal` 与 `place`。
6. 系统上报 `abnormal_ready` 后，允许用户发送 `return_home`。
7. 收到 `cruise` / `phase=entered` 后，认为返航完成。

## 10. Python 接入示例

```python
import json
import ssl
import time
import paho.mqtt.client as mqtt

BROKER = "x3a5a71f.ala.cn-hangzhou.emqxsl.cn"
PORT = 8883
USERNAME = "spic"
PASSWORD = "lhr20041005"
CA_FILE = r"E:\esp32\allproject\codeproject\SmartCarMode\host_pc\certs\emqxsl-ca.pem"

TOPIC_SMARTCAR_CMD = "smartcar/cmd"
TOPIC_SYSTEM_STATUS = "smartcar/system/status"
TOPIC_CAMERA_JPEG = "cam/jpeg"


def on_connect(client, userdata, flags, rc):
    print("connected", rc)
    client.subscribe(TOPIC_SYSTEM_STATUS, qos=1)
    client.subscribe(TOPIC_CAMERA_JPEG, qos=0)


def on_message(client, userdata, msg):
    if msg.topic == TOPIC_CAMERA_JPEG:
        if msg.payload.startswith(b"\xff\xd8"):
            print("jpeg frame", len(msg.payload))
        return

    text = msg.payload.decode("utf-8", errors="ignore")
    print(msg.topic, text)


client = mqtt.Client(client_id=f"third_host_{int(time.time())}")
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(ca_certs=CA_FILE, cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

payload = {
    "cmd": "system",
    "state": "abnormal",
    "place": "bathroom",
    "requestId": f"sys_{int(time.time() * 1000)}",
}
client.publish(TOPIC_SMARTCAR_CMD, json.dumps(payload, ensure_ascii=False), qos=1, retain=False)

input("press Enter to return home...")
client.publish(
    TOPIC_SMARTCAR_CMD,
    json.dumps({"cmd": "system", "state": "return_home", "requestId": f"sys_{int(time.time() * 1000)}"}),
    qos=1,
    retain=False,
)
```

## 11. 兼容性与约束

- 系统状态命令推荐发布到 `smartcar/cmd`，不要直接拆分控制 Voice 和 Camera。
- `cam/jpeg` 是二进制 JPEG，不是 JSON。
- `smartcar/system/status` 是 retained 消息，新上位机连接后可收到最近一次系统状态。
- `return_home` 依赖 SmartCar 记录的上一次异常地点，建议只在 `abnormal_ready` 后发送。
- `requestId` 当前主要用于上位机追踪，设备侧不会强制校验唯一性。
- 中文文本命令仅作为兼容能力，第三方上位机应优先使用英文 JSON 字段。

