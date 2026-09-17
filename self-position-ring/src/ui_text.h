#pragma once
#include <array>

namespace self_ring::ui {
// Register every visible string with Nexus so its managed font atlas includes our glyphs.
inline constexpr const char* Title = "自身定位圈";
inline constexpr const char* Window = "自身定位圈###SelfPositionRingSettings";
inline constexpr const char* Intro = "在自己脚下显示圆圈，帮助你在战场人群中找到自己。";
inline constexpr const char* Enabled = "显示圆圈";
inline constexpr const char* Radius = "圆圈大小";
inline constexpr const char* Thickness = "线条粗细";
inline constexpr const char* Outline = "描边粗细";
inline constexpr const char* Opacity = "不透明度";
inline constexpr const char* Colour = "圆圈颜色";
inline constexpr const char* OutlineColour = "描边颜色";
inline constexpr const char* ColourHelp = "点击色板选颜色，拖动右侧彩虹条切换色相。";
inline constexpr const char* Presets = "常用颜色";
inline constexpr const char* Done = "完成";
inline constexpr const char* Restore = "恢复默认";
inline constexpr const char* Open = "打开独立设置面板";
inline constexpr const char* Preview = "外观预览";
inline constexpr const char* Footnote = "拖动滑块立即生效，松手后自动保存。战斗内外均显示。";
inline constexpr const char* Hidden = "当前已关闭圆圈，勾选上方开关即可显示。";
inline constexpr const char* SaveError = "设置保存失败，请检查游戏目录是否可写。当前调整仍然生效。";
inline constexpr const char* PositionWaiting = "等待游戏角色位置；进入地图后自动显示。";
inline constexpr const char* FontWaiting = "正在加载中文字体…";
inline constexpr const char* About = "关于与开源许可";
inline constexpr const char* AboutText = "自身定位圈 1.0.0\n参考 Range Indicators 的位置投影思路；使用 Nexus 接口与 Noto Sans SC 中文字体。";
inline constexpr const char* UnitRadius = "%.0f 游戏单位";
inline constexpr const char* UnitPixel = "%.1f 像素";
inline constexpr const char* Cyan = "青色";
inline constexpr const char* Yellow = "黄色";
inline constexpr const char* Green = "绿色";
inline constexpr const char* White = "白色";
inline constexpr const char* Pink = "粉色";
inline constexpr const char* Black = "黑色";
inline constexpr std::array All{
    Title, Window, Intro, Enabled, Radius, Thickness, Outline, Opacity, Colour,
    OutlineColour, ColourHelp, Presets, Done, Restore, Open, Preview, Footnote,
    Hidden, SaveError, PositionWaiting, FontWaiting, About, AboutText,
    UnitRadius, UnitPixel, Cyan, Yellow, Green, White, Pink, Black
};
} // namespace self_ring::ui
