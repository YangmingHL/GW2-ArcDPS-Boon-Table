# 自身定位圈 / Self Position Ring

独立的 Windows x64 Nexus 插件，在自己脚下显示带描边的圆圈，帮助在 WvW 人群中找到自己。

## 安装与设置

1. 游戏中已安装并启用 [Nexus](https://raidcore.gg/Nexus)。
2. 关闭游戏，把 `self_position_ring.dll` 放进游戏的 `addons` 目录。
3. 启动游戏后默认显示青色黑边圆圈，首次运行自动打开中文设置面板。
4. 之后可右键 Nexus 图标选择“自身定位圈”；也可在 Nexus 的插件设置中找到 `Self Position Ring`。

大小、线宽、描边和不透明度均使用滑块；点击颜色色块打开可视化取色器。
拖动立即生效，松手后自动保存到 `addons/SelfPositionRing/settings.ini`。
“恢复默认”会重新开启青色黑边圆圈。圈本身不拦截鼠标。

无需安装 Range Indicators 或 ArcDPS。中文字体与第三方许可内置 DLL，许可可在“关于与开源许可”查看。
若已安装 RealTime API，则优先读取其有效、同地图的位置；否则使用 MumbleLink。
打开大地图、离开游戏场景或位置数据无效时隐藏。没有对 WvW 的模式限制。

圆圈随人物和镜头投影变化，线宽按屏幕像素计算。它是水平的覆盖层，不会沿坡面贴地。
不读取其他玩家信息，不修改游戏，也不自动执行技能。

## 构建

使用带 C++ 桌面开发组件的 Visual Studio 与 CMake 3.24+：

```sh
cmake -S self-position-ring -B cmake-build/self-position-ring -A x64
cmake --build cmake-build/self-position-ring --config Release
ctest --test-dir cmake-build/self-position-ring -C Release --output-on-failure
```

产物为 `cmake-build/self-position-ring/Release/self_position_ring.dll`。
依赖固定到具体提交；字体与 SDK 文件在配置时下载并验证 SHA-256。
使用静态 C++ 运行库，不需要单独分发 C++ 运行库或字体。

GitHub Actions 工作流：`.github/workflows/build_self_position_ring.yml`。
测试包括投影/裁剪、设置持久化，以及 Windows 中实际加载 DLL、中文字符、模拟 WvW 绘制和卸载检查。
这些检查不能代替 Windows 游戏内的实战体验验证。

## 来源

- [Range Indicators](https://github.com/RaidcoreGG/GW2-RangeIndicators)：参考位置投影与 Nexus 集成思路，MIT。
- [Nexus API](https://github.com/RaidcoreGG/RCGG-lib-nexus-api) 与 [Mumble API](https://github.com/RaidcoreGG/RCGG-lib-mumble-api)：MIT。
- [Dear ImGui](https://github.com/RaidcoreGG/imgui)：使用 Nexus 兼容的 1.80 分支，MIT。
- [Noto Sans SC](https://github.com/notofonts/noto-cjk)：SIL Open Font License 1.1。
- [RealTime API](https://github.com/RaidcoreGG/GW2-RealTime-API-Releases)：可选宿主资源，仅使用公开接口，不分发其插件。
