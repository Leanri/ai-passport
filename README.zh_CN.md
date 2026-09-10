<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# FoloToy AI Passport 接鸡蛋游戏

这是一款受经典掌上液晶游戏启发的轻量接鸡蛋游戏。设备开机后直接进入游戏，并使用适配屏幕尺寸的图像；高分辨率源图不会载入设备内存。

## 操作

- `UP`：让狐狸和篮筐转向左边的母鸡。
- `DOWN`：让狐狸和篮筐转向右边的母鸡。
- 在开始或游戏结束画面按任意一个按键即可开玩。
- 长按 `OK`：退出游戏并返回标题画面。
- 在标题画面短按 `OK`：重新进入游戏。

游戏会记录分数，允许三次失误，并随分数提升逐渐加速；电量计可用时还会显示电池百分比。

## 目标硬件

- 搭载 ESP32-C3 的 FoloToy AI Passport
- 8 MB Flash，无 PSRAM
- 240 × 320 ST7789P3 屏幕
- 三个 ADC 按键（`UP`、`DOWN` 和 `OK`）

原有的 3 MB 应用上限和位于 `0x356000` 的受保护 `cardid` 分区保持不变。切勿整片擦除已经写入设备身份信息的机器。

## 构建与刷写

请使用 ESP-IDF 5.5.3。通过验证的完整固件输出为 `build/FoloToy-AI-Passport-full.bin`。对于已经写入身份信息的设备，优先使用分段式 `idf.py flash`，避免触碰受保护区域。

准确命令和安全检查请参阅上游[构建指南](docs/development/engineering/build-and-test.zh_CN.md)。
