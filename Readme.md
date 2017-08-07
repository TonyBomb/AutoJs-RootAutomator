# Root-Automator

此项目旨在解决shell命令中sendevent, input tap, input swipe等命令"执行过慢"的问题。使Auto.js使用root权限的触摸录制的回放更流畅。

## How it works 

不管是sendevent, input tap还是input swipe，每次执行单条命令都会打开一次设备文件(/dev/input/event*)并写入相应的数据再关闭文件。如果用他们来执行一系列的动作，动作之间有明显的不连续。触摸录制的回放效果很差。

因而，这个项目只做了一点微小的工作，让同一个脚本的每个动作都使用同一个文件描述符，仅此而已。

## How to use

把bin目录下的可执行文件root_automator复制或者adb push到有可执行权限的目录(例如/data/local/tmp，以下以此目录为例)，用shell命令执行
```
chmod 777 /data/local/tmp/root_automator
/data/local/tmp/root_automator -d out_devive_path \[-w currentDeviceWidth -h currentDeviceHeight\] in_file_path
```
其中root_automator的参数意义如下：
* in_file_path 一个描述触摸事件以及事件之间的延迟的特定格式的文件，参见[输入文件格式](#输入文件格式)。
* out_device_path 输出的触摸设备路径，例如/dev/input/event1。至于如何获取触摸设备的路径，可以用getevent命令。
* w 当前设备宽度，可选项。用于自动根据录制时的设备宽度放缩坐标。
* h 当前设备高度，可选项。用于自动根据录制时的设备高度放缩坐标。

注意，自动放缩坐标是试验性功能。因为屏幕放向改变时坐标放缩也应相应改变，但在root_automator中难以实时获取当前屏幕方向。因此，放缩功能只对始终在竖屏方向录制的脚本有效。

示例：
```
/data/local/tmp/root_automator -d /dev/input/event1 /sdcard/record.auto
```

## 输入文件格式

程序会按照以下步骤解析输入文件：
1. 读取文件头，共256个字节。依次为：
* 0x00~0x03 固定值0x00B87B6D
* 0x04~0x07 一个32位整数值，表示录制文件的版本
* 0x08~0x0b 一个32位整数值，表示脚本录制时所用设备的宽度
* 0x0c~0x0f 一个32位整数值，表示脚本录制时所用设备的高度
* 0x10~0xff 保留字节。

2. 读取一个字节(data_type)。根据data_type的值：
	* 0x00 读取后面4个字节作为一个int值(n)。暂停执行n毫秒的时间。
	* 0x01 读取后面8个字节构造一个input_event(参见[linux/input.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input.h))。把他写入到out_device_path设备中。其中input_event的时间戳time会被置为0，type取自0\~1字节，code取自2\~3字节，value取自4\~7字节)
	* 0x02 向设备写入一个type, code, value均为0的input_event，这个事件是SYNC_REPORT。
	* 0x03 向设备写入一个type为0x03, code为0x35, value为之后4个字节表示的整数值的input_event，这个事件是ABS_MT_POSITION_X。
	* 0x04 向设备写入一个type为0x03, code为0x36, value为之后4个字节表示的整数值的input_event，这个事件是ABS_MT_POSITION_Y。

其中0x02~0x04的数据类型的目的为减少录制文件大小。

3. 如果到达文件末尾，结束程序；否则执行步骤2。

### 更多

参见Auto.js项目中的[RootAutomator](https://github.com/hyb1996/NoRootScriptDroid/blob/master/autojs/src/main/java/com/stardust/autojs/runtime/api/RootAutomator.java)以及[RootAutomatorEngine](https://github.com/hyb1996/NoRootScriptDroid/blob/master/autojs/src/main/java/com/stardust/autojs/engine/RootAutomatorEngine.java)。