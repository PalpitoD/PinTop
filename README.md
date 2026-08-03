# PinTop

轻量级窗口置顶工具，基于 [DeskPins](https://efotinis.neocities.org/deskpins/index.html) 的
理念用现代 C++（C++23）从零重构：

- **64 位**，仅支持 Windows 10 / 11
- **零依赖**：单 exe（静态链接运行库），任何机器即开即用，无需安装 VC++ Redistributable
- **轻量**：常驻仅一个隐藏消息窗口 + 托盘图标，图钉窗口按需创建
- **高 DPI**：Per-Monitor V2 感知，高分屏不模糊不错位
- **CI 构建**：GitHub Actions 自动编译 x64 Release，产物从 Actions artifact 下载

## 构建

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

产物：`build\Release\PinTop.exe`

## 功能状态

- [x] 托盘图标 + 右键菜单（含开机自启开关）
- [x] 放置图钉（置顶窗口）：图层捕获选择目标，无合成鼠标输入
- [x] 移除图钉（点击图钉 / 目标窗口销毁）
- [x] 图钉跟随窗口移动（SetWinEventHook）
- [x] SVG 图钉：WIC 解码带 alpha 位图 + UpdateLayeredWindow 抗锯齿
- [x] 应用/托盘图标：SVG 渲染的多尺寸 app.ico（16~256px）
- [x] 热键（Ctrl+Shift+P 快速置顶/取消置顶前台窗口）
- [ ] 自动置顶规则
- [ ] 选项对话框

## 旧版参考

原始 DeskPins 代码归档在 `PinTop-initial-baseline/third_party/deskpins/`，
仅供逻辑参考，不参与构建。
