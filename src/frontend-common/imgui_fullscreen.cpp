#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui_fullscreen.h"
#include "IconsFontAwesome5.h"
#include "common/assert.h"
#include "common/file_system.h"
#include "common/string.h"
#include "common/string_util.h"
#include "core/host_display.h"
#include "core/host_interface.h"
#include "imgui_internal.h"
#include "imgui_styles.h"
#include <cmath>
#include <mutex>

namespace ImGuiFullscreen {
static void DrawFileSelector();
static void DrawChoiceDialog();
static void DrawBackgroundProgressDialogs();

ImFont* g_standard_font = nullptr;
ImFont* g_medium_font = nullptr;
ImFont* g_large_font = nullptr;
ImFont* g_icon_font = nullptr;

float g_layout_scale = 1.0f;
float g_layout_padding_left = 0.0f;
float g_layout_padding_top = 0.0f;

static float s_menu_bar_size;
static std::string s_font_filename;
static std::string s_icon_font_filename;
static std::vector<u8> s_icon_font_data;
static float s_font_size = 15.0f;
static const ImWchar* s_font_glyph_range = nullptr;

static u32 s_menu_button_index = 0;

static ImRect PadRect(const ImRect& r, float padding)
{
  return ImRect(ImVec2(r.Min.x + padding, r.Min.y + padding), ImVec2(r.Max.x - padding, r.Max.y - padding));
}

void SetFontFilename(const char* filename)
{
  if (filename)
    s_font_filename = filename;
  else
    std::string().swap(s_font_filename);
}

void SetIconFontFilename(const char* icon_font_filename)
{
  if (icon_font_filename)
    s_icon_font_filename = icon_font_filename;
  else
    std::string().swap(s_icon_font_filename);
}

void SetIconFontData(std::vector<u8> data)
{
  s_icon_font_data = std::move(data);
}

void SetFontSize(float size_pixels)
{
  s_font_size = size_pixels;
}

void SetFontGlyphRanges(const ImWchar* glyph_ranges)
{
  s_font_glyph_range = glyph_ranges;
}

void SetMenuBarSize(float size)
{
  if (s_menu_bar_size == size)
    return;

  s_menu_bar_size = size;
}

static void AddIconFonts(float size)
{
  static const ImWchar range_fa[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

  ImFontConfig cfg;
  cfg.MergeMode = true;
  cfg.PixelSnapH = true;
  cfg.GlyphMinAdvanceX = size * 0.75f;
  cfg.GlyphMaxAdvanceX = size * 0.75f;

  if (!s_icon_font_data.empty())
  {
    cfg.FontDataOwnedByAtlas = false;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(s_icon_font_data.data(), static_cast<int>(s_icon_font_data.size()),
                                               size * 0.75f, &cfg, range_fa);
  }
  else
  {
    ImGui::GetIO().Fonts->AddFontFromFileTTF(s_icon_font_filename.c_str(), size * 0.75f, &cfg, range_fa);
  }
}

bool UpdateFonts()
{
  const float standard_font_size = std::round(DPIScale(s_font_size));
  const float medium_font_size = std::round(LayoutScale(LAYOUT_MEDIUM_FONT_SIZE));
  const float large_font_size = std::round(LayoutScale(LAYOUT_LARGE_FONT_SIZE));

  if (g_standard_font && g_standard_font->FontSize == standard_font_size && medium_font_size &&
      g_medium_font->FontSize == medium_font_size && large_font_size && g_large_font->FontSize == large_font_size)
  {
    return false;
  }

  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();

  if (s_font_filename.empty())
  {
    g_standard_font = ImGui::AddRobotoRegularFont(standard_font_size);
    AddIconFonts(standard_font_size);
    g_medium_font = ImGui::AddRobotoRegularFont(medium_font_size);
    AddIconFonts(medium_font_size);
    g_large_font = ImGui::AddRobotoRegularFont(large_font_size);
    AddIconFonts(large_font_size);
  }
  else
  {
    g_standard_font =
      io.Fonts->AddFontFromFileTTF(s_font_filename.c_str(), standard_font_size, nullptr, s_font_glyph_range);
    AddIconFonts(standard_font_size);
    g_medium_font =
      io.Fonts->AddFontFromFileTTF(s_font_filename.c_str(), medium_font_size, nullptr, s_font_glyph_range);
    AddIconFonts(medium_font_size);
    g_large_font = io.Fonts->AddFontFromFileTTF(s_font_filename.c_str(), large_font_size, nullptr, s_font_glyph_range);
    AddIconFonts(large_font_size);
  }

  if (!io.Fonts->Build())
    Panic("Failed to rebuild font atlas");

  return true;
}

bool UpdateLayoutScale()
{
  static constexpr float LAYOUT_RATIO = LAYOUT_SCREEN_WIDTH / LAYOUT_SCREEN_HEIGHT;
  const ImGuiIO& io = ImGui::GetIO();

  const float screen_width = io.DisplaySize.x;
  const float screen_height = io.DisplaySize.y - s_menu_bar_size;
  const float screen_ratio = screen_width / screen_height;
  const float old_scale = g_layout_scale;

  if (screen_ratio > LAYOUT_RATIO)
  {
    // screen is wider, use height, pad width
    g_layout_scale = screen_height / LAYOUT_SCREEN_HEIGHT;
    g_layout_padding_top = s_menu_bar_size;
    g_layout_padding_left = (screen_width - (LAYOUT_SCREEN_WIDTH * g_layout_scale)) / 2.0f;
  }
  else
  {
    // screen is taller, use width, pad height
    g_layout_scale = screen_width / LAYOUT_SCREEN_WIDTH;
    g_layout_padding_top = (screen_height - (LAYOUT_SCREEN_HEIGHT * g_layout_scale)) / 2.0f + s_menu_bar_size;
    g_layout_padding_left = 0.0f;
  }

  return g_layout_scale != old_scale;
}

void BeginLayout()
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor());
  ImGui::PushStyleColor(ImGuiCol_Button, UIPrimaryLineColor());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, UISecondaryDarkColor());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UISecondaryColor());
  ImGui::PushStyleColor(ImGuiCol_Border, UISecondaryLightColor());
}

void EndLayout()
{
  DrawFileSelector();
  DrawChoiceDialog();
  DrawBackgroundProgressDialogs();

  ImGui::PopStyleColor(5);
  ImGui::PopStyleVar(2);
}

bool BeginFullscreenColumns(const char* title)
{
  ImGui::SetNextWindowPos(ImVec2(g_layout_padding_left, g_layout_padding_top));
  ImGui::SetNextWindowSize(LayoutScale(ImVec2(LAYOUT_SCREEN_WIDTH, LAYOUT_SCREEN_HEIGHT)));

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  bool clipped;
  if (title)
  {
    ImGui::PushFont(g_large_font);
    clipped = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::PopFont();
  }
  else
  {
    clipped = ImGui::Begin("fullscreen_ui_columns_parent", nullptr,
                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
  }

  return clipped;
}

void EndFullscreenColumns()
{
  ImGui::End();
  ImGui::PopStyleVar(3);
}

bool BeginFullscreenColumnWindow(float start, float end, const char* name, const ImVec4& background)
{
  const ImVec2 pos(LayoutScale(start), 0.0f);
  const ImVec2 size(LayoutScale(ImVec2(end - start, LAYOUT_SCREEN_HEIGHT)));

  ImGui::PushStyleColor(ImGuiCol_ChildBg, background);

  ImGui::SetCursorPos(pos);

  return ImGui::BeginChild(name, size, false, ImGuiWindowFlags_NavFlattened);
}

void EndFullscreenColumnWindow()
{
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

bool BeginFullscreenWindow(float left, float top, float width, float height, const char* name,
                           const ImVec4& background /* = HEX_TO_IMVEC4(0x212121, 0xFF) */, float rounding /*= 0.0f*/,
                           float padding /*= 0.0f*/, ImGuiWindowFlags flags /*= 0*/)
{
  if (left < 0.0f)
    left = (LAYOUT_SCREEN_WIDTH - width) * -left;
  if (top < 0.0f)
    top = (LAYOUT_SCREEN_HEIGHT - height) * -top;

  ImGui::SetNextWindowSize(LayoutScale(ImVec2(width, height)));
  ImGui::SetNextWindowPos(ImVec2(LayoutScale(left) + g_layout_padding_left, LayoutScale(top) + g_layout_padding_top));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, background);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(padding, padding));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(rounding));

  return ImGui::Begin(name, nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoBringToFrontOnFocus | flags);
}

void EndFullscreenWindow()
{
  ImGui::End();
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor();
}

void BeginMenuButtons(u32 num_items, float y_align, float x_padding, float y_padding, float item_height)
{
  s_menu_button_index = 0;

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(x_padding, y_padding));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

  if (y_align != 0.0f)
  {
    const float total_size =
      static_cast<float>(num_items) * LayoutScale(item_height + (y_padding * 2.0f)) + LayoutScale(y_padding * 2.0f);
    const float window_height = ImGui::GetWindowHeight();
    if (window_height > total_size)
      ImGui::SetCursorPosY((window_height - total_size) * y_align);
  }
}

void EndMenuButtons()
{
  ImGui::PopStyleVar(4);
}

void DrawWindowTitle(const char* title)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImVec2 pos(window->DC.CursorPos + LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
  const ImVec2 size(window->WorkRect.GetWidth() - (LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING) * 2.0f),
                    g_large_font->FontSize + LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f);
  const ImRect rect(pos, pos + size);

  ImGui::ItemSize(size);
  if (!ImGui::ItemAdd(rect, window->GetID("window_title")))
    return;

  ImGui::PushFont(g_large_font);
  ImGui::RenderTextClipped(rect.Min, rect.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &rect);
  ImGui::PopFont();

  const ImVec2 line_start(pos.x, pos.y + g_large_font->FontSize + LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING));
  const ImVec2 line_end(pos.x + size.x, line_start.y);
  const float line_thickness = LayoutScale(1.0f);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddLine(line_start, line_end, IM_COL32(255, 255, 255, 255), line_thickness);
}

static void GetMenuButtonFrameBounds(float height, ImVec2* pos, ImVec2* size)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  *pos = window->DC.CursorPos;
  *size = ImVec2(window->WorkRect.GetWidth(), LayoutScale(height) + ImGui::GetStyle().FramePadding.y * 2.0f);
}

static bool MenuButtonFrame(const char* str_id, bool enabled, float height, bool* visible, bool* hovered, ImRect* bb,
                            ImGuiButtonFlags flags = 0)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImVec2 pos, size;
  GetMenuButtonFrameBounds(height, &pos, &size);
  *bb = ImRect(pos, pos + size);

  const ImGuiID id = window->GetID(str_id);
  ImGui::ItemSize(size);
  if (enabled)
  {
    if (!ImGui::ItemAdd(*bb, id))
    {
      *visible = false;
      *hovered = false;
      return false;
    }
  }
  else
  {
    if (ImGui::IsClippedEx(*bb, id, false))
    {
      *visible = false;
      *hovered = false;
      return false;
    }
  }

  *visible = true;

  bool held;
  bool pressed;
  if (enabled)
  {
    pressed = ImGui::ButtonBehavior(*bb, id, hovered, &held, flags);
    if (*hovered)
    {
      const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
      ImGui::RenderFrame(bb->Min, bb->Max, col, true, 0.0f);
    }
  }
  else
  {
    pressed = false;
    held = false;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  bb->Min += style.FramePadding;
  bb->Max -= style.FramePadding;

  return pressed;
}

bool MenuButtonFrame(const char* str_id, bool enabled, float height, bool* visible, bool* hovered, ImVec2* min,
                     ImVec2* max, ImGuiButtonFlags flags /*= 0*/)
{
  ImRect bb;
  const bool result = MenuButtonFrame(str_id, enabled, height, visible, hovered, &bb, flags);
  *min = bb.Min;
  *max = bb.Max;
  return result;
}

void MenuHeading(const char* title, bool draw_line /*= true*/)
{
  const float line_thickness = draw_line ? LayoutScale(1.0f) : 0.0f;
  const float line_padding = draw_line ? LayoutScale(5.0f) : 0.0f;

  bool visible, hovered;
  ImRect bb;
  MenuButtonFrame("menu_heading", false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, &visible, &hovered, &bb);
  if (!visible)
    return;

  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
  ImGui::PushFont(g_large_font);
  ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
  ImGui::PopFont();
  ImGui::PopStyleColor();

  if (draw_line)
  {
    const ImVec2 line_start(bb.Min.x, bb.Min.y + g_large_font->FontSize + line_padding);
    const ImVec2 line_end(bb.Max.x, line_start.y);
    ImGui::GetWindowDrawList()->AddLine(line_start, line_end, ImGui::GetColorU32(ImGuiCol_TextDisabled));
  }
}

bool ActiveButton(const char* title, bool is_active, bool enabled, float height, ImFont* font)
{
  if (is_active)
  {
    ImVec2 pos, size;
    GetMenuButtonFrameBounds(height, &pos, &size);
    ImGui::RenderFrame(pos, pos + size, ImGui::GetColorU32(UIPrimaryColor()), false);
  }

  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  const ImRect title_bb(bb.GetTL(), bb.GetBR());

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::PopFont();

  if (!enabled)
    ImGui::PopStyleColor();

  s_menu_button_index++;
  return pressed;
}

bool MenuButton(const char* title, const char* summary, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
  const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
  const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  s_menu_button_index++;
  return pressed;
}

bool MenuImageButton(const char* title, const char* summary, ImTextureID user_texture_id, const ImVec2& image_size,
                     bool enabled, float height, const ImVec2& uv0, const ImVec2& uv1, ImFont* title_font,
                     ImFont* summary_font)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  ImGui::GetWindowDrawList()->AddImage(user_texture_id, bb.Min, bb.Min + image_size, uv0, uv1,
                                       enabled ? IM_COL32(255, 255, 255, 255) :
                                                 ImGui::GetColorU32(ImGuiCol_TextDisabled));

  const float midpoint = bb.Min.y + title_font->FontSize + LayoutScale(4.0f);
  const float text_start_x = bb.Min.x + image_size.x + LayoutScale(15.0f);
  const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
  const ImRect summary_bb(ImVec2(text_start_x, midpoint), bb.Max);

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(title_font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  s_menu_button_index++;
  return pressed;
}

bool ToggleButton(const char* title, const char* summary, bool* v, bool enabled, float height, ImFont* font,
                  ImFont* summary_font)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb, ImGuiButtonFlags_PressedOnClick);
  if (!visible)
    return false;

  const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
  const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
  const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  const float toggle_width = LayoutScale(50.0f);
  const float toggle_height = LayoutScale(25.0f);
  const float toggle_x = LayoutScale(8.0f);
  const float toggle_y = (LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT) - toggle_height) * 0.5f;
  const float toggle_radius = toggle_height * 0.5f;
  const ImVec2 toggle_pos(bb.Max.x - toggle_width - toggle_x, bb.Min.y + toggle_y);

  if (pressed)
    *v = !*v;

  float t = *v ? 1.0f : 0.0f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImGuiContext& g = *GImGui;
  float ANIM_SPEED = 0.08f;
  if (g.LastActiveId == g.CurrentWindow->GetID(title)) // && g.LastActiveIdTimer < ANIM_SPEED)
  {
    float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
    t = *v ? (t_anim) : (1.0f - t_anim);
  }

  ImU32 col_bg;
  if (!enabled)
    col_bg = IM_COL32(0x75, 0x75, 0x75, 0xff);
  else if (hovered)
    col_bg = ImGui::GetColorU32(ImLerp(HEX_TO_IMVEC4(0x9e9e9e, 0xff), UISecondaryLightColor(), t));
  else
    col_bg = ImGui::GetColorU32(ImLerp(HEX_TO_IMVEC4(0x757575, 0xff), UISecondaryLightColor(), t));

  dl->AddRectFilled(toggle_pos, ImVec2(toggle_pos.x + toggle_width, toggle_pos.y + toggle_height), col_bg,
                    toggle_height * 0.5f);
  dl->AddCircleFilled(
    ImVec2(toggle_pos.x + toggle_radius + t * (toggle_width - toggle_radius * 2.0f), toggle_pos.y + toggle_radius),
    toggle_radius - 1.5f, IM_COL32(255, 255, 255, 255), 32);

  s_menu_button_index++;
  return pressed;
}

bool RangeButton(const char* title, const char* summary, s32* value, s32 min, s32 max, s32 increment,
                 const char* format, bool enabled /*= true*/, float height /*= LAYOUT_MENU_BUTTON_HEIGHT*/,
                 ImFont* font /*= g_large_font*/, ImFont* summary_font /*= g_medium_font*/)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  TinyString value_text;
  value_text.Format(format, *value);
  const ImVec2 value_size(ImGui::CalcTextSize(value_text));

  const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
  const float text_end = bb.Max.x - value_size.x;
  const ImRect title_bb(bb.Min, ImVec2(text_end, midpoint));
  const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), ImVec2(text_end, bb.Max.y));

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::RenderTextClipped(bb.Min, bb.Max, value_text, nullptr, nullptr, ImVec2(1.0f, 0.5f), &bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  if (pressed)
    ImGui::OpenPopup(title);

  bool changed = false;

  ImGui::SetNextWindowSize(LayoutScale(300.0f, 120.0f));

  if (ImGui::BeginPopupModal(title, nullptr))
  {
    ImGui::SetNextItemWidth(LayoutScale(300.0f));
    changed = ImGui::SliderInt("##value", value, min, max, format, ImGuiSliderFlags_NoInput);

    BeginMenuButtons();
    if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
      ImGui::CloseCurrentPopup();
    EndMenuButtons();

    ImGui::EndPopup();
  }

  return changed;
}

bool RangeButton(const char* title, const char* summary, float* value, float min, float max, float increment,
                 const char* format, bool enabled /*= true*/, float height /*= LAYOUT_MENU_BUTTON_HEIGHT*/,
                 ImFont* font /*= g_large_font*/, ImFont* summary_font /*= g_medium_font*/)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  TinyString value_text;
  value_text.Format(format, *value);
  const ImVec2 value_size(ImGui::CalcTextSize(value_text));

  const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
  const float text_end = bb.Max.x - value_size.x;
  const ImRect title_bb(bb.Min, ImVec2(text_end, midpoint));
  const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), ImVec2(text_end, bb.Max.y));

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::RenderTextClipped(bb.Min, bb.Max, value_text, nullptr, nullptr, ImVec2(1.0f, 0.5f), &bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  if (pressed)
    ImGui::OpenPopup(title);

  bool changed = false;

  ImGui::SetNextWindowSize(LayoutScale(300.0f, 120.0f));

  if (ImGui::BeginPopupModal(title, nullptr))
  {
    ImGui::SetNextItemWidth(LayoutScale(300.0f));
    changed = ImGui::SliderFloat("##value", value, min, max, format, ImGuiSliderFlags_NoInput);

    BeginMenuButtons();
    if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
      ImGui::CloseCurrentPopup();
    EndMenuButtons();

    ImGui::EndPopup();
  }

  return changed;
}

bool MenuButtonWithValue(const char* title, const char* summary, const char* value, bool enabled, float height,
                         ImFont* font, ImFont* summary_font)
{
  ImRect bb;
  bool visible, hovered;
  bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
  if (!visible)
    return false;

  const ImVec2 value_size(ImGui::CalcTextSize(value));

  const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
  const float text_end = bb.Max.x - value_size.x;
  const ImRect title_bb(bb.Min, ImVec2(text_end, midpoint));
  const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), ImVec2(text_end, bb.Max.y));

  if (!enabled)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

  ImGui::PushFont(font);
  ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
  ImGui::RenderTextClipped(bb.Min, bb.Max, value, nullptr, nullptr, ImVec2(1.0f, 0.5f), &bb);
  ImGui::PopFont();

  if (summary)
  {
    ImGui::PushFont(summary_font);
    ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f),
                             &summary_bb);
    ImGui::PopFont();
  }

  if (!enabled)
    ImGui::PopStyleColor();

  return pressed;
}

static ImGuiID s_enum_choice_button_id = 0;
static s32 s_enum_choice_button_value = 0;
static bool s_enum_choice_button_set = false;

bool EnumChoiceButtonImpl(const char* title, const char* summary, s32* value_pointer,
                          const char* (*to_display_name_function)(s32 value, void* opaque), void* opaque, u32 count,
                          bool enabled, float height, ImFont* font, ImFont* summary_font)
{
  const bool pressed = MenuButtonWithValue(title, summary, to_display_name_function(*value_pointer, opaque), enabled,
                                           height, font, summary_font);

  if (pressed)
  {
    s_enum_choice_button_id = ImGui::GetID(title);
    s_enum_choice_button_value = *value_pointer;
    s_enum_choice_button_set = false;

    ChoiceDialogOptions options;
    options.reserve(count);
    for (u32 i = 0; i < count; i++)
      options.emplace_back(to_display_name_function(static_cast<s32>(i), opaque), *value_pointer == i);
    OpenChoiceDialog(title, false, std::move(options), [](s32 index, const std::string& title, bool checked) {
      if (index >= 0)
        s_enum_choice_button_value = index;

      s_enum_choice_button_set = true;
      CloseChoiceDialog();
    });
  }

  bool changed = false;
  if (s_enum_choice_button_set && s_enum_choice_button_id == ImGui::GetID(title))
  {
    changed = s_enum_choice_button_value != *value_pointer;
    if (changed)
      *value_pointer = s_enum_choice_button_value;

    s_enum_choice_button_id = 0;
    s_enum_choice_button_value = 0;
    s_enum_choice_button_set = false;
  }

  return changed;
}

struct FileSelectorItem
{
  FileSelectorItem() = default;
  FileSelectorItem(std::string display_name_, std::string full_path_, bool is_file_)
    : display_name(std::move(display_name_)), full_path(std::move(full_path_)), is_file(is_file_)
  {
  }
  FileSelectorItem(const FileSelectorItem&) = default;
  FileSelectorItem(FileSelectorItem&&) = default;
  ~FileSelectorItem() = default;

  FileSelectorItem& operator=(const FileSelectorItem&) = default;
  FileSelectorItem& operator=(FileSelectorItem&&) = default;

  std::string display_name;
  std::string full_path;
  bool is_file;
};

static bool s_file_selector_open = false;
static bool s_file_selector_directory = false;
static std::string s_file_selector_title;
static FileSelectorCallback s_file_selector_callback;
static std::string s_file_selector_current_directory;
static std::vector<std::string> s_file_selector_filters;
static std::vector<FileSelectorItem> s_file_selector_items;

static void PopulateFileSelectorItems()
{
  s_file_selector_items.clear();

  if (s_file_selector_current_directory.empty())
  {
    for (std::string& root_path : FileSystem::GetRootDirectoryList())
    {
      s_file_selector_items.emplace_back(StringUtil::StdStringFromFormat(ICON_FA_FOLDER "  %s", root_path.c_str()),
                                         std::move(root_path), false);
    }
  }
  else
  {
    FileSystem::FindResultsArray results;
    FileSystem::FindFiles(s_file_selector_current_directory.c_str(), "*",
                          FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES |
                            FILESYSTEM_FIND_RELATIVE_PATHS,
                          &results);

    std::string parent_path;
    std::string::size_type sep_pos = s_file_selector_current_directory.rfind(FS_OSPATH_SEPARATOR_CHARACTER);
    if (sep_pos != std::string::npos)
    {
      parent_path = s_file_selector_current_directory.substr(0, sep_pos);
      FileSystem::CanonicalizePath(parent_path, true);
    }

    s_file_selector_items.emplace_back(ICON_FA_FOLDER_OPEN "  <Parent Directory>", std::move(parent_path), false);
    std::sort(results.begin(), results.end(), [](const FILESYSTEM_FIND_DATA& lhs, const FILESYSTEM_FIND_DATA& rhs) {
      if ((lhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY) !=
          (rhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY))
        return (lhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY) != 0;

      // return std::lexicographical_compare(lhs.FileName.begin(), lhs.FileName.end(), rhs.FileName.begin(),
      // rhs.FileName.end());
      return (StringUtil::Strcasecmp(lhs.FileName.c_str(), rhs.FileName.c_str()) < 0);
    });

    for (const FILESYSTEM_FIND_DATA& fd : results)
    {
      std::string full_path(StringUtil::StdStringFromFormat(
        "%s" FS_OSPATH_SEPARATOR_STR "%s", s_file_selector_current_directory.c_str(), fd.FileName.c_str()));

      if (fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
      {
        std::string title(StringUtil::StdStringFromFormat(ICON_FA_FOLDER "  %s", fd.FileName.c_str()));
        s_file_selector_items.emplace_back(std::move(title), std::move(full_path), false);
      }
      else
      {
        if (s_file_selector_filters.empty() ||
            std::none_of(s_file_selector_filters.begin(), s_file_selector_filters.end(),
                         [&fd](const std::string& filter) {
                           return StringUtil::WildcardMatch(fd.FileName.c_str(), filter.c_str());
                         }))
        {
          continue;
        }

        std::string title(StringUtil::StdStringFromFormat(ICON_FA_FILE "  %s", fd.FileName.c_str()));
        s_file_selector_items.emplace_back(std::move(title), std::move(full_path), true);
      }
    }
  }
}

static void SetFileSelectorDirectory(std::string dir)
{
  while (!dir.empty() && dir.back() == FS_OSPATH_SEPARATOR_CHARACTER)
    dir.erase(dir.size() - 1);

  s_file_selector_current_directory = std::move(dir);
  PopulateFileSelectorItems();
}

void OpenFileSelector(const char* title, bool select_directory, FileSelectorCallback callback,
                      FileSelectorFilters filters, std::string initial_directory)
{
  if (s_file_selector_open)
    CloseFileSelector();

  s_file_selector_open = true;
  s_file_selector_directory = select_directory;
  s_file_selector_title = StringUtil::StdStringFromFormat("%s##file_selector", title);
  s_file_selector_callback = std::move(callback);
  s_file_selector_filters = std::move(filters);

  if (initial_directory.empty() || !FileSystem::DirectoryExists(initial_directory.c_str()))
    initial_directory = FileSystem::GetWorkingDirectory();
  SetFileSelectorDirectory(std::move(initial_directory));
}

void CloseFileSelector()
{
  if (!s_file_selector_open)
    return;

  s_file_selector_open = false;
  s_file_selector_directory = false;
  std::string().swap(s_file_selector_title);
  FileSelectorCallback().swap(s_file_selector_callback);
  FileSelectorFilters().swap(s_file_selector_filters);
  std::string().swap(s_file_selector_current_directory);
  s_file_selector_items.clear();
  ImGui::CloseCurrentPopup();
}

void DrawFileSelector()
{
  if (!s_file_selector_open)
    return;

  ImGui::SetNextWindowSize(LayoutScale(1000.0f, 680.0f));
  ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::OpenPopup(s_file_selector_title.c_str());

  FileSelectorItem* selected = nullptr;

  ImGui::PushFont(g_large_font);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));

  bool is_open = (ImGui::GetNavInputAmount(ImGuiNavInput_Cancel, ImGuiInputReadMode_Pressed) < 1.0f);
  bool directory_selected = false;
  if (ImGui::BeginPopupModal(s_file_selector_title.c_str(), &is_open,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
  {
    BeginMenuButtons();

    if (!s_file_selector_current_directory.empty())
    {
      MenuButton(TinyString::FromFormat(ICON_FA_FOLDER_OPEN "  %s", s_file_selector_current_directory.c_str()), nullptr,
                 false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
    }

    if (s_file_selector_directory && !s_file_selector_current_directory.empty())
    {
      if (MenuButton(ICON_FA_FOLDER_PLUS "  <Use This Directory>", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
        directory_selected = true;
    }

    SmallString title;
    for (FileSelectorItem& item : s_file_selector_items)
    {
      if (MenuButton(item.display_name.c_str(), nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
        selected = &item;
    }

    EndMenuButtons();

    ImGui::EndPopup();
  }
  else
  {
    is_open = false;
  }

  ImGui::PopStyleVar(2);
  ImGui::PopFont();

  if (selected)
  {
    if (selected->is_file)
    {
      s_file_selector_callback(selected->full_path);
    }
    else
    {
      SetFileSelectorDirectory(std::move(selected->full_path));
    }
  }
  else if (directory_selected)
  {
    s_file_selector_callback(s_file_selector_current_directory);
  }
  else if (!is_open)
  {
    std::string no_path;
    s_file_selector_callback(no_path);
    CloseFileSelector();
  }
}

static bool s_choice_dialog_open = false;
static bool s_choice_dialog_checkable = false;
static std::string s_choice_dialog_title;
static ChoiceDialogOptions s_choice_dialog_options;
static ChoiceDialogCallback s_choice_dialog_callback;

void OpenChoiceDialog(const char* title, bool checkable, ChoiceDialogOptions options, ChoiceDialogCallback callback)
{
  if (s_choice_dialog_open)
    CloseChoiceDialog();

  s_choice_dialog_open = true;
  s_choice_dialog_checkable = checkable;
  s_choice_dialog_title = StringUtil::StdStringFromFormat("%s##choice_dialog", title);
  s_choice_dialog_options = std::move(options);
  s_choice_dialog_callback = std::move(callback);
}

void CloseChoiceDialog()
{
  if (!s_choice_dialog_open)
    return;

  s_choice_dialog_open = false;
  s_choice_dialog_checkable = false;
  std::string().swap(s_choice_dialog_title);
  ChoiceDialogOptions().swap(s_choice_dialog_options);
  ChoiceDialogCallback().swap(s_choice_dialog_callback);
}

void DrawChoiceDialog()
{
  if (!s_choice_dialog_open)
    return;

  const float width = 600.0f;
  const float title_height =
    g_large_font->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
  const float height =
    std::min(400.0f, title_height + (LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + (LAYOUT_MENU_BUTTON_Y_PADDING * 2.0f)) *
                                      static_cast<float>(s_choice_dialog_options.size()));
  ImGui::SetNextWindowSize(LayoutScale(width, height));
  ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::OpenPopup(s_choice_dialog_title.c_str());

  ImGui::PushFont(g_large_font);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));

  bool is_open = (ImGui::GetNavInputAmount(ImGuiNavInput_Cancel, ImGuiInputReadMode_Pressed) < 1.0f);
  s32 choice = -1;
  bool choice_checked = false;

  if (ImGui::BeginPopupModal(s_choice_dialog_title.c_str(), &is_open,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
  {
    BeginMenuButtons();

    if (s_choice_dialog_checkable)
    {
      SmallString title;
      for (s32 i = 0; i < static_cast<s32>(s_choice_dialog_options.size()); i++)
      {
        auto& option = s_choice_dialog_options[i];

        title.Format("%s  %s", option.second ? ICON_FA_CHECK_SQUARE : ICON_FA_SQUARE, option.first.c_str());
        if (MenuButton(title, nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
        {
          choice = i;
          option.second = !option.second;
        }
      }
    }
    else
    {
      for (s32 i = 0; i < static_cast<s32>(s_choice_dialog_options.size()); i++)
      {
        auto& option = s_choice_dialog_options[i];
        SmallString title;
        if (option.second)
          title.AppendString(ICON_FA_CHECK "  ");
        title.AppendString(option.first);

        if (ActiveButton(title, option.second, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
        {
          choice = i;
          for (s32 j = 0; j < static_cast<s32>(s_choice_dialog_options.size()); j++)
            s_choice_dialog_options[j].second = (j == i);
        }
      }
    }

    EndMenuButtons();

    ImGui::EndPopup();
  }
  else
  {
    is_open = false;
  }

  ImGui::PopStyleVar(2);
  ImGui::PopFont();

  if (choice >= 0)
  {
    const auto& option = s_choice_dialog_options[choice];
    s_choice_dialog_callback(choice, option.first, option.second);
  }
  else if (!is_open)
  {
    std::string no_string;
    s_choice_dialog_callback(-1, no_string, false);
    CloseChoiceDialog();
  }
}

struct BackgroundProgressDialogData
{
  std::string message;
  ImGuiID id;
  s32 min;
  s32 max;
  s32 value;
};

static std::vector<BackgroundProgressDialogData> s_background_progress_dialogs;
static std::mutex s_background_progress_lock;

static ImGuiID GetBackgroundProgressID(const char* str_id)
{
  return ImHashStr(str_id);
}

void OpenBackgroundProgressDialog(const char* str_id, std::string message, s32 min, s32 max, s32 value)
{
  const ImGuiID id = GetBackgroundProgressID(str_id);

  std::unique_lock<std::mutex> lock(s_background_progress_lock);

  for (const BackgroundProgressDialogData& data : s_background_progress_dialogs)
  {
    Assert(data.id != id);
  }

  BackgroundProgressDialogData data;
  data.id = id;
  data.message = std::move(message);
  data.min = min;
  data.max = max;
  data.value = value;
  s_background_progress_dialogs.push_back(std::move(data));
}

void UpdateBackgroundProgressDialog(const char* str_id, std::string message, s32 min, s32 max, s32 value)
{
  const ImGuiID id = GetBackgroundProgressID(str_id);

  std::unique_lock<std::mutex> lock(s_background_progress_lock);

  for (BackgroundProgressDialogData& data : s_background_progress_dialogs)
  {
    if (data.id == id)
    {
      data.message = std::move(message);
      data.min = min;
      data.max = max;
      data.value = value;
      return;
    }
  }

  Panic("Updating unknown progress entry.");
}

void CloseBackgroundProgressDialog(const char* str_id)
{
  const ImGuiID id = GetBackgroundProgressID(str_id);

  std::unique_lock<std::mutex> lock(s_background_progress_lock);

  for (auto it = s_background_progress_dialogs.begin(); it != s_background_progress_dialogs.end(); ++it)
  {
    if (it->id == id)
    {
      s_background_progress_dialogs.erase(it);
      return;
    }
  }

  Panic("Closing unknown progress entry.");
}

void DrawBackgroundProgressDialogs()
{
  std::unique_lock<std::mutex> lock(s_background_progress_lock);
  if (s_background_progress_dialogs.empty())
    return;

  const float window_width = LayoutScale(500.0f);
  const float window_height = LayoutScale(75.0f);
  const float window_spacing = LayoutScale(20.0f);

  float current_pos_x = LayoutScale(10.0f);
  float current_pos_y = (LayoutScale(LAYOUT_SCREEN_HEIGHT) + g_layout_padding_top) - window_height - LayoutScale(10.0f);

  ImGui::PushStyleColor(ImGuiCol_WindowBg, UIPrimaryDarkColor());
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, UISecondaryLightColor());
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, LayoutScale(4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, LayoutScale(1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(10.0f, 10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LayoutScale(10.0f, 10.0f));
  ImGui::PushFont(g_medium_font);

  ImDrawList* dl = ImGui::GetForegroundDrawList();

  for (const BackgroundProgressDialogData& data : s_background_progress_dialogs)
  {
    dl->AddRectFilled(ImVec2(current_pos_x, current_pos_y),
                      ImVec2(current_pos_x + window_width, current_pos_y + window_height),
                      IM_COL32(0x11, 0x11, 0x11, 200), LayoutScale(10.0f));

    ImVec2 pos(current_pos_x + LayoutScale(10.0f), current_pos_y + LayoutScale(10.0f));
    dl->AddText(g_medium_font, g_medium_font->FontSize, pos, IM_COL32(255, 255, 255, 255), data.message.c_str(),
                nullptr, 0.0f);
    pos.y += g_medium_font->FontSize + LayoutScale(10.0f);

    ImVec2 bar_end(pos.x + window_width - LayoutScale(10.0f * 2.0f), pos.y + LayoutScale(25.0f));
    float fraction = static_cast<float>(data.value - data.min) / static_cast<float>(data.max - data.min);
    dl->AddRectFilled(pos, bar_end, ImGui::GetColorU32(UIPrimaryDarkColor()));
    dl->AddRectFilled(pos, ImVec2(pos.x + fraction * (bar_end.x - pos.x), bar_end.y),
                      ImGui::GetColorU32(UISecondaryColor()));

    TinyString text;
    text.Format("%d%%", static_cast<int>(std::round(fraction * 100.0f)));
    const ImVec2 text_size(ImGui::CalcTextSize(text));
    const ImVec2 text_pos(pos.x + ((bar_end.x - pos.x) / 2.0f) - (text_size.x / 2.0f),
                          pos.y + ((bar_end.y - pos.y) / 2.0f) - (text_size.y / 2.0f));
    dl->AddText(g_medium_font, g_medium_font->FontSize, text_pos, ImGui::GetColorU32(UIPrimaryTextColor()), text);

    current_pos_y -= window_height - window_spacing;
  }

  ImGui::PopFont();
  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(2);
}

} // namespace ImGuiFullscreen