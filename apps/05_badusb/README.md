# 05 — BadUSB HID 键盘注入器

> ⚠️ **仅限在你自己拥有/已获授权的设备上测试**。键盘注入工具属双用途设备，请遵守当地法律与测试授权，不要用于未经许可的机器。

## 用途

把 RP2350 变成 USB HID 键盘：读取 SD 卡根目录的 `.txt` 脚本（Duckyscript 风格），通过 USB_OTG 口向目标电脑注入按键。LCD 菜单选择脚本，KEY1 物理按键触发，调试串口可看注入日志。

## 接线 / 用法

- **USB_OTG（USB0）** → 目标电脑（注入对象）
- **CH343 调试串口（COM5）** → 你的开发机（菜单控制 + 日志）
- SD 卡根目录放 `.txt` 脚本，插卡后开机自动扫描

菜单操作：

- **KEY1**：注入当前选中的脚本
- 串口命令：`n` 下一个 / `p` 上一个 / `e` 注入当前 / `s` 重新扫描 / `r`（或 `reboot`）进入 bootloader

## 脚本语法

每行一条命令，大小写不敏感，`#` 或 `REM` 开头为注释：

| 命令 | 说明 |
| --- | --- |
| `STRING <text>` | 逐字输入文本（自动处理 Shift） |
| `STRINGLN <text>` | 输入文本后回车 |
| `ENTER` / `TAB` / `ESC` / `SPACE` / `BS` | 单键 |
| `DELETE` / `HOME` / `END` / `PGUP` / `PGDN` | 编辑键 |
| `UP` / `DOWN` / `LEFT` / `RIGHT`（或 `*ARROW`） | 方向键 |
| `CAPSLOCK` / `CAPS` | 大小写锁定 |
| `F1` ~ `F12` | 功能键 |
| `CTRL <键...>` / `ALT` / `GUI` / `SHIFT` | 组合键，支持多键：`CTRL ALT DEL`、`ALT TAB`、`GUI r` |
| `DELAY <ms>` / `WAIT <ms>` | 延迟（最大 59999ms） |
| `DEFAULTDELAY <ms>` | 设置行间默认延迟（0 = 关闭） |
| `SHIFT` / `CTRL` / `ALT` / `GUI`（单独一行） | 单击修饰键（如切换输入法模式） |

> **中文输入法问题（重要）**：如果目标机活动输入法是中文拼音（如微软拼音），注入的字母会被输入法截走去组词（如 `was` → `哇塞`）、空格被组词确认吃掉、符号变全角，导致文本乱码。
>
> **推荐解法（示例脚本已内置）**：脚本开头加一行 `ALT SHIFT`，把键盘布局切换到英文（英文布局没有拼音组词）。注意：
> - `ALT SHIFT` 是**切换键盘布局**（全局生效，跨窗口保持）；而单独的 `SHIFT` 只切换输入法的 中/英 **模式**，该模式**按窗口（输入上下文）记忆**——在旧窗口敲 SHIFT 切到英文，Win+R 打开的运行对话框会回到中文模式，照样乱码。所以要用 `ALT SHIFT` 而不是 `SHIFT`。
> - 若目标机本来就是英文布局，`ALT SHIFT` 会把它切回中文——此时删掉脚本里的 `ALT SHIFT` 行。
> - 备选方案：目标机默认输入法固定为英文键盘；或把注入文本全部用大写（大写字母不受拼音组词影响）。

示例（自动打开记事本并输入）：

```text
REM === Demo ===
DEFAULTDELAY 50
GUI r
DELAY 300
STRING notepad
ENTER
DELAY 500
STRING Hello from RP2350 BadUSB!
ENTER
```

## 构建 / 烧录

```powershell
.\build.ps1 05_badusb
.\flash.ps1 .\apps\05_badusb\build-arm\05_badusb.uf2
```

首次上电会自动在 SD 卡写入示例脚本 `/hello.txt`。
