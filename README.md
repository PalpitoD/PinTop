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

- [x] 托盘图标：左键单击进入置顶模式，右键菜单（置顶/退出）
- [x] 放置图钉（置顶窗口）：图层捕获选择目标，无合成鼠标输入，图钉光标
- [x] 移除图钉（点击图钉 / 目标窗口销毁）
- [x] 图钉跟随窗口移动（SetWinEventHook）；最小化保持置顶、恢复后图钉重现
- [x] SVG 图钉：预生成 premultiplied BGRA + UpdateLayeredWindow 抗锯齿
- [x] 应用/托盘图标与置顶光标：SVG 渲染（多尺寸 app.ico / placepin.cur）
- [ ] 自动置顶规则
- [ ] 选项对话框

## 旧版参考

原始 DeskPins 代码归档在 `PinTop-initial-baseline/third_party/deskpins/`，
仅供逻辑参考，不参与构建。
