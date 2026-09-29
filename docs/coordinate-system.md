# 数控车床四轴坐标系统定义

本文按照实际车床操作习惯定义 X、Y、Z、C 四个坐标轴。这里不再假设“绝对坐标一定等于机械坐标减刀补”，而是把机械坐标、工件坐标、刀具坐标和显示坐标分层保存。

## 1. 轴方向定义

机床采用右手笛卡尔结构：

| 轴 | 含义 | 正方向 |
|---|---|---|
| X | 径向坐标 | 主轴中心以上为正，以下为负 |
| Y | 高度/横向坐标 | 按实际机床方向配置 |
| Z | 主轴轴向坐标 | 远离主轴为正，靠近主轴为负 |
| C | 主轴旋转轴 | 绕 Z 轴旋转，角度制 |

关键约定：

- 主轴上的工件零点可以人为设置。
- 工件零点默认可以设在主轴端面和主轴中心。
- 靠近主轴的 Z 坐标为负。
- X 轴跨越主轴中心时，正负号按照“中心以上为正、中心以下为负”处理。
- 车床通常没有实际 Y 轴，但数据结构保留 Y，方便后续车铣复合。
- C 轴表示主轴角度，不是普通直线轴。

## 2. 四层坐标

### 2.1 机械坐标 MCS

机械坐标是人为定义的机床公共坐标。

数据库保存：

```text
machine_x
machine_y
machine_z
machine_c
```

特点：

- 轴没有移动时，机械坐标不能因为换刀而改变。
- 换刀只会改变刀具几何偏置和绝对坐标显示。
- 机械坐标范围可以为负，例如回转刀塔常见的大致范围为：

```text
X: -300 ~ +300 mm
Z: -300 ~ +300 mm
C: 0 ~ 360 deg
```

示例中的 `(-200, 200)` 只是近似范围，具体范围应由机床参数配置。

### 2.2 工件坐标 WCS

工件坐标是主轴上的工件零点。

可以设置：

```text
work_origin_machine_x
work_origin_machine_y
work_origin_machine_z
work_origin_machine_c
```

默认工件坐标可以设置为主轴端面中心：

```text
X = 0
Y = 0
Z = 0
C = 0
```

工件坐标支持多组：

- G54
- G55
- G56
- G57
- G58
- G59

### 2.3 刀具坐标 TCS

回转刀塔有 12 个刀位，每把刀独立保存：

```text
tool_no
tool_type
geometry_x
geometry_y
geometry_z
wear_x
wear_y
wear_z
machine_setup_x
machine_setup_y
machine_setup_z
measured_x
measured_y
measured_z
nose_radius
nose_wear
tip_direction
tool_angle
```

每把刀可以有独立的机械位置范围：

```text
min_machine_x
max_machine_x
min_machine_y
max_machine_y
min_machine_z
max_machine_z
```

刀具实际刀尖位置：

```text
toolTipMachineX = machineX + geometryX + wearX
toolTipMachineY = machineY + geometryY + wearY
toolTipMachineZ = machineZ + geometryZ + wearZ
```

换刀时：

```text
machineX / machineY / machineZ 不变
geometryX / geometryY / geometryZ 切换
absoluteX / absoluteY / absoluteZ 重新计算
```

### 2.3.1 X 轴对刀测量

X 轴采用试切测量：

1. 调出刀号和刀补号，例如 `T0101`。
2. 用这把刀试车工件外圆。
3. 保持 X 轴当前位置不变，记录当前机械坐标 `machineSetupX`。
4. 用卡尺测量试车直径，例如 `49.500`。
5. 输入 `X49.500`，选择“测量”。
6. 系统结合当前工件坐标、直径/半径模式和磨耗，自动计算形状偏置。

X 轴已经确认按直径保存，因此不需要乘 2：

```text
geometryX = measuredX + workOriginX - machineSetupX - wearX
```

`OFFSET -> 形状` 页面应显示：

```text
刀号
对刀机械坐标 X
测量直径 X
计算后的形状偏置 X
当前磨耗 X
```

X 轴测量命令：

```text
MEASURE X49.5
```

系统自动记录当前机械坐标，并把 `49.5` 保存为当前刀的形状测量值。

加工时：

```text
toolTipMachineX = machineX + geometryX + wearX
absoluteDisplayX = toolTipMachineX - workOriginX
```

### 2.3.2 Z 轴对刀测量

Z 轴是距离轴，可以使用端面、卡盘端面或标准块作为参照：

1. 刀具接触 Z 轴基准。
2. 保持 Z 轴位置不变，记录 `machineSetupZ`。
3. 输入该位置对应的绝对距离，例如 `Z0` 或 `Z100`。
4. 系统反算 Z 轴形状偏置：

```text
geometryZ = measuredZ + workOriginZ - machineSetupZ - wearZ
```

Z 轴测量命令：

```text
MEASURE Z0
```

`OFFSET -> 形状` 页面应显示：

```text
对刀机械坐标 Z
测量距离 Z
计算后的形状偏置 Z
磨耗 Z
```

### 2.3.3 Y 轴对刀测量

Y 轴表示高度。带 Y 轴车床或车铣复合可以按照高度基准测量：

```text
geometryY = measuredY + workOriginY - machineSetupY - wearY
```

普通两轴车床可以把 Y 固定为 `0`，后续启用 Y 轴时再进入测量流程。

### 2.3.4 对刀记录的数据库字段

每次测量保存原始数据：

```text
machine_setup_x
machine_setup_y
machine_setup_z
measured_x
measured_y
measured_z
```

这些字段用于形状偏置页面的追溯和重新计算。

加工时使用：

```text
geometry_x / geometry_y / geometry_z
wear_x / wear_y / wear_z
```

### 2.4 显示坐标 DCS

显示坐标包括：

- 机械坐标
- 绝对坐标
- 相对坐标
- C 轴角度

## 3. 绝对坐标

绝对坐标是刀尖相对于工件零点的加工尺寸：

```text
absoluteX = toolTipMachineX - workOriginMachineX
absoluteY = toolTipMachineY - workOriginMachineY
absoluteZ = toolTipMachineZ - workOriginMachineZ
absoluteC = machineC - workOriginMachineC
```

对于车床 X 轴：

如果内部保存的是半径值，而屏幕显示直径：

```text
absoluteDisplayX = 2 * absoluteX
```

如果内部保存的就是直径值：

```text
absoluteDisplayX = absoluteX
```

例如：

```text
工件外圆 50
加工到 49.5
屏幕 ABS X 应该显示 49.500
```

## 4. 相对坐标

相对坐标不参与刀具补偿，是机械坐标相对于一个可清零基准的显示：

```text
relativeX = machineX - relativeOriginMachineX
relativeY = machineY - relativeOriginMachineY
relativeZ = machineZ - relativeOriginMachineZ
relativeC = normalizeAngle(machineC - relativeOriginMachineC)
```

换刀时：

- 机械坐标不变
- 相对坐标不变
- 绝对坐标根据当前刀的几何和磨耗重新计算

相对坐标清零：

```text
REL 0 X
REL 0 Y
REL 0 Z
REL 0 C
REL ALL 0
```

## 5. C 轴定义

C 轴控制主轴的旋转角度。

保存状态：

```text
c_angle
c_direction
c_mode
```

其中：

- `c_angle`：0 到 360 度
- `c_direction`：正转或反转
- `c_mode`：连续旋转、定位、分度、主轴模式

角度归一化：

```text
C = fmod(angle, 360)
if C < 0:
    C = C + 360
```

显示时应同时显示：

```text
ABS C
REL C
C 当前角度
C 定位目标
```

## 6. 数据库结构建议

### machine_state

```text
id
machine_x
machine_y
machine_z
machine_c
active_tool
work_offset_no
x_display_mode
unit
display_decimals
```

### work_offsets

```text
offset_no
work_origin_machine_x
work_origin_machine_y
work_origin_machine_z
work_origin_machine_c
```

### tools

```text
tool_no
tool_type
geometry_x
geometry_y
geometry_z
wear_x
wear_y
wear_z
nose_radius
nose_wear
tip_direction
tool_angle
min_machine_x
max_machine_x
min_machine_y
max_machine_y
min_machine_z
max_machine_z
```

### relative_state

```text
relative_origin_machine_x
relative_origin_machine_y
relative_origin_machine_z
relative_origin_machine_c
```

### c_axis_state

```text
c_angle
c_target_angle
c_direction
c_mode
```

## 7. 完整计算链

```text
机械坐标 M(X,Y,Z,C)
        +
刀具几何偏置 G(X,Y,Z)
        +
刀具磨耗 W(X,Y,Z)
        =
刀尖机械坐标 T(X,Y,Z)
        -
工件零点 WCS(X,Y,Z,C)
        =
绝对坐标 ABS(X,Y,Z,C)
        +
直径/半径和单位转换
        =
屏幕显示
```

相对坐标单独计算：

```text
机械坐标 - 相对坐标零点
```

## 8. 待确认参数

在改数据库和代码之前，需要最后确认：

1. Y+ 方向是向前、向后还是向上？
2. C+ 是顺时针还是逆时针？
3. X 已经确认按直径保存。
4. 工件 zero 的 Z=0 在卡盘端面还是工件端面？
5. 刀塔几何偏置的符号约定是什么？
6. 刀塔每把刀的机械坐标范围是否统一？
7. 是否需要为每把刀单独设置机械范围？
8. 显示保留三位还是四位小数？
