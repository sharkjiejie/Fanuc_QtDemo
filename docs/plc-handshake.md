# M 代码与 PLC 握手

真实机床的 M 代码响应通常在毫秒级完成。立即响应不等于没有握手，而是 CNC 扫描周期和 PLC 扫描周期都很快。

## 执行顺序

```text
CNC 读取 M03
  → 立即输出 M03 请求和 STROBE
  → PLC 控制主轴正转
  → PLC 返回 FIN
  → CNC 清除 STROBE
  → 继续执行下一程序段
```

## 为什么不能使用 sleep

不能写：

```cpp
QThread::sleep(1);
```

这会阻塞界面和事件循环，也会把真实机床不需要的等待强加给程序。

正确方式是事件驱动：

```text
Idle
  → Requested
  → WaitingForFin
  → Completed
  → NextInstruction
```

每次定时器扫描只检查一次 PLC 状态，不阻塞。

## 当前项目实现

`PlcSimulator` 保存：

```text
mCode
mStrobe
mFin
spindleForward
spindleReverse
spindleStopped
coolantOn
highPressurePumpOn
scanCount
```

当前仿真 PLC 在一个扫描周期内返回 FIN，因此视觉上等同于立即响应。

M28 是程序段瞬时输出：

```text
当前段有 M28        → 高压泵油打开
下一段也有 M28      → 保持打开
下一段没有 M28      → 自动关闭
暂停/急停/复位/M30 → 强制关闭
```

## 将来接真实 PLC

把 `PlcSimulator::scan()` 替换成真实通信读取：

- Modbus TCP 寄存器
- EtherCAT PDO
- OPC UA 节点
- PLC 共享内存

信号结构不变，只需把仿真 FIN 改成读取真实完成位，并增加超时报警。
