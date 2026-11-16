# emoji/

气泡表情包。**不随仓库分发**（第三方素材，版权原因），缺失时只有气泡功能被禁用，
其余功能（待机 / 行走 / 点击 / 扫头 / 拖拽）完全不受影响。

## 文件约定

- 文件名 `emoji_01.png`、`emoji_02.png` …… 从 **01 开始连续**；
  中间断号时，程序在第一个缺失处停止加载（和动作帧的规则一致）
- 格式 PNG，**带 alpha 通道**（透明背景）
- 尺寸任意，程序会按 `pet.ini` 的 `[bubble] emoji_size_px`（默认 220px）等比缩放

## 怎么放

推荐用仓库里的脚本，它会自动编号、等比缩放，不必手动改名：

```powershell
python tools/make_emoji.py D:\stickers                 # 目录下所有图片/GIF
python tools/make_emoji.py D:\stickers\*.png
python tools/make_emoji.py D:\stickers\funny.gif       # 动图取第一帧
python tools/make_emoji.py D:\stickers --dry-run       # 先看会怎么编号
```

手动按上面的文件名规则放进本目录也可以。

## 运行时表现

- **有表情包**：每 20~60 秒随机弹出一张，显示约 3 秒，气泡是独立的点击穿透窗口
- **没有**：启动日志里是 `[pet] no emoji stickers, bubble disabled`，
  程序照常运行，只是不弹气泡

> 启动日志在 GUI 程序里看不到（没有控制台）。判断方法很简单：等她一分钟，
> 头顶没弹过气泡就是没加载到；或者看本目录里有没有 `emoji_01.png`。
