#include <pch.h>
#include <core/fonts/sourcesans.h>
#include <thirdparty/imgui/misc/imgui_utility.h>
#include <thirdparty/imgui/misc/cpp/imgui_stdlib.h>

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils\utils.h>

#define ImGuiReadSetting(str, var, a, type)  if (sscanf_s(line, str, &a) == 1) { var = static_cast<type>(a); }

extern ExportSettings_t g_ExportSettings;
extern PreviewSettings_t g_PreviewSettings;

// asset settings
static void* AssetSettings_ReadOpen(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, const char* const name)
{
    UNUSED(handler);
    UNUSED(ctx);

    for (auto& it : g_assetData.m_assetTypeBindings)
    {
        std::string asset = fourCCToString(it.first);
        if (strcmp(asset.c_str(), name) == NULL)
        {
            return &it.second.e.exportSetting;
        }
    }

    return nullptr;
}

static void AssetSettings_ReadLine(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, void* const entry, const char* const line)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (entry)
    {
        int* const exportSetting = static_cast<int*>(entry);

        int i;
        ImGuiReadSetting("Setting=%i", *exportSetting, i, int);
    }
}

static void AssetSettings_WriteAll(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, ImGuiTextBuffer* const buf)
{
    UNUSED(ctx);

    buf->reserve(buf->size() + 50);
    for (auto& it : g_assetData.m_assetTypeBindings)
    {
        buf->appendf("[%s][%s]\n", handler->TypeName, fourCCToString(it.first).c_str());
        buf->appendf("Setting=%d\n", it.second.e.exportSetting);
        buf->append("\n");
    }
}

// utils settings
static void* UtilSettings_ReadOpen(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, const char* const name)
{
    UNUSED(handler);
    UNUSED(ctx);
    UNUSED(name);

    return UtilsConfig;
}

static void UtilSettings_ReadLine(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, void* const entry, const char* const line)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (entry)
    {
        ImGuiHandler::UtilsSettings_t* const cfg = static_cast<ImGuiHandler::UtilsSettings_t*>(entry);

        uint32_t i;
        ImGuiReadSetting("ExportThreads=%u", cfg->exportThreadCount, i, uint32_t);
        ImGuiReadSetting("ParseThreads=%u", cfg->parseThreadCount, i, uint32_t);
        ImGuiReadSetting("CompressionLevel=%u", cfg->compressionLevel, i, uint32_t);

        int checkUpdates = 0;
        if (sscanf_s(line, "CheckForUpdatesOnStartup=%d", &checkUpdates) == 1)
            cfg->checkForUpdatesOnStartup = checkUpdates != 0;
    }
}

static void UtilSettings_WriteAll(ImGuiContext* const ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* const buf)
{
    UNUSED(ctx);

    buf->reserve(buf->size() + 64);
    buf->appendf("[%s][utils]\n", handler->TypeName );
    buf->appendf("ExportThreads=%u\n", UtilsConfig->exportThreadCount);
    buf->appendf("ParseThreads=%u\n", UtilsConfig->parseThreadCount);
    buf->appendf("CompressionLevel=%u\n", UtilsConfig->compressionLevel);
    buf->appendf("CheckForUpdatesOnStartup=%d\n", UtilsConfig->checkForUpdatesOnStartup ? 1 : 0);
    buf->append("\n");
}

// theme settings
static void* ThemeSettings_ReadOpen(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, const char* const name)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (strcmp(name, "theme") == 0)
        return &g_pImGuiHandler->theme;
    return nullptr;
}

static void ThemeSettings_ReadLine(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, void* const entry, const char* const line)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (entry)
    {
        ImGuiHandler::ThemeSettings_t* const theme = static_cast<ImGuiHandler::ThemeSettings_t*>(entry);

        // Helper to read ImVec4
        auto readColor = [line](const char* label, ImVec4& color) {
            float r, g, b, a;
            if (sscanf_s(line, label, &r, &g, &b, &a) == 4) {
                color.x = r; color.y = g; color.z = b; color.w = a;
            }
        };

        readColor("AccentColor=%f,%f,%f,%f", theme->accentColor);
        readColor("AccentHovered=%f,%f,%f,%f", theme->accentHovered);
        readColor("AccentActive=%f,%f,%f,%f", theme->accentActive);
        readColor("WindowBg=%f,%f,%f,%f", theme->windowBg);
        readColor("HeaderBg=%f,%f,%f,%f", theme->headerBg);
        readColor("TitleBg=%f,%f,%f,%f", theme->titleBg);
        readColor("TextRegular=%f,%f,%f,%f", theme->textRegular);
    }
}

static void ThemeSettings_WriteAll(ImGuiContext* const ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* const buf)
{
    UNUSED(ctx);

    buf->reserve(buf->size() + 256);
    buf->appendf("[%s][theme]\n", handler->TypeName);

    const auto& t = g_pImGuiHandler->theme;
    buf->appendf("AccentColor=%.4f,%.4f,%.4f,%.4f\n", t.accentColor.x, t.accentColor.y, t.accentColor.z, t.accentColor.w);
    buf->appendf("AccentHovered=%.4f,%.4f,%.4f,%.4f\n", t.accentHovered.x, t.accentHovered.y, t.accentHovered.z, t.accentHovered.w);
    buf->appendf("AccentActive=%.4f,%.4f,%.4f,%.4f\n", t.accentActive.x, t.accentActive.y, t.accentActive.z, t.accentActive.w);
    buf->appendf("WindowBg=%.4f,%.4f,%.4f,%.4f\n", t.windowBg.x, t.windowBg.y, t.windowBg.z, t.windowBg.w);
    buf->appendf("HeaderBg=%.4f,%.4f,%.4f,%.4f\n", t.headerBg.x, t.headerBg.y, t.headerBg.z, t.headerBg.w);
    buf->appendf("TitleBg=%.4f,%.4f,%.4f,%.4f\n", t.titleBg.x, t.titleBg.y, t.titleBg.z, t.titleBg.w);
    buf->appendf("TextRegular=%.4f,%.4f,%.4f,%.4f\n", t.textRegular.x, t.textRegular.y, t.textRegular.z, t.textRegular.w);
    buf->append("\n");
}

// export settings
static void* ExportSettings_ReadOpen(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, const char* const name)
{
    UNUSED(handler);
    UNUSED(ctx);
    UNUSED(name);

    return &g_ExportSettings;
}

static void ExportSettings_ReadLine(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, void* const entry, const char* const line)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (entry)
    {
        ExportSettings_t* const settings = static_cast<ExportSettings_t*>(entry);

        int i;
        ImGuiReadSetting("ExportPathsFull=%i",              settings->exportPathsFull, i, int);
        ImGuiReadSetting("ExportAssetDeps=%i",              settings->exportAssetDeps, i, int);
        ImGuiReadSetting("ExportAssetDependents=%i",        settings->exportAssetDependents, i, int);
        ImGuiReadSetting("DisableCacheNames=%i",            settings->disableCachedNames, i, int);


        ImGuiReadSetting("ExportTextureNameSetting=%u",     settings->exportTextureNameSetting, i, uint32_t);
        ImGuiReadSetting("ExportNormalRecalcSetting=%u",    settings->exportNormalRecalcSetting, i, uint32_t);
        ImGuiReadSetting("ExportMaterialTextures=%i",       settings->exportMaterialTextures, i, int);

        ImGuiReadSetting("QCMajorVersion=%u",               settings->qcMajorVersion, i, uint16_t);
        ImGuiReadSetting("QCMinorVersion=%u",               settings->qcMinorVersion, i, uint16_t);

        ImGuiReadSetting("ExportRigSequences=%i",           settings->exportRigSequences, i, int);
        ImGuiReadSetting("ExportModelSkin=%i",              settings->exportModelSkin, i, int);
        ImGuiReadSetting("ExportTruncatedMaterials=%i",     settings->exportModelMatsTruncated, i, int);
        ImGuiReadSetting("ExportQCIFiles=%i",               settings->exportQCIFiles, i, int);
    }
}

static void ExportSettings_WriteAll(ImGuiContext* const ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* const buf)
{
    UNUSED(ctx);

    buf->reserve(buf->size() + (48 * 11));
    buf->appendf("[%s][general]\n", handler->TypeName);
    
    buf->appendf("ExportPathsFull=%i\n",            g_ExportSettings.exportPathsFull);
    buf->appendf("ExportAssetDeps=%i\n",            g_ExportSettings.exportAssetDeps);
    buf->appendf("ExportAssetDependents=%i\n",      g_ExportSettings.exportAssetDependents);
    buf->appendf("DisableCacheNames=%i\n",          g_ExportSettings.disableCachedNames);

    buf->appendf("ExportTextureNameSetting=%u\n",   g_ExportSettings.exportTextureNameSetting);
    buf->appendf("ExportNormalRecalcSetting=%u\n",  g_ExportSettings.exportNormalRecalcSetting);
    buf->appendf("ExportMaterialTextures=%i\n",     g_ExportSettings.exportMaterialTextures);

    buf->appendf("QCMajorVersion=%u\n",             g_ExportSettings.qcMajorVersion);
    buf->appendf("QCMinorVersion=%u\n",             g_ExportSettings.qcMinorVersion);

    buf->appendf("ExportRigSequences=%i\n",         g_ExportSettings.exportRigSequences);
    buf->appendf("ExportModelSkin=%i\n",            g_ExportSettings.exportModelSkin);
    buf->appendf("ExportTruncatedMaterials=%i\n",   g_ExportSettings.exportModelMatsTruncated);
    buf->appendf("ExportQCIFiles=%i\n",             g_ExportSettings.exportQCIFiles);

    buf->appendf("\n");
}

// preview settings
static void* PreviewSettings_ReadOpen(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, const char* const name)
{
    UNUSED(handler);
    UNUSED(ctx);
    UNUSED(name);

    return &g_PreviewSettings;
}

static void PreviewSettings_ReadLine(ImGuiContext* const ctx, ImGuiSettingsHandler* const handler, void* const entry, const char* const line)
{
    UNUSED(handler);
    UNUSED(ctx);

    if (entry)
    {
        PreviewSettings_t* const settings = static_cast<PreviewSettings_t*>(entry);

        float i;
        ImGuiReadSetting("PreviewCullDistance=%f",  settings->previewCullDistance, i, float);
        ImGuiReadSetting("PreviewMovementSpeed=%f", settings->previewMovementSpeed, i, float);
    }
}

static void PreviewSettings_WriteAll(ImGuiContext* const ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* const buf)
{
    UNUSED(ctx);

    buf->reserve(buf->size() + 128);
    buf->appendf("[%s][general]\n", handler->TypeName);

    buf->appendf("PreviewCullDistance=%f\n",    g_PreviewSettings.previewCullDistance);
    buf->appendf("PreviewMovementSpeed=%f\n",   g_PreviewSettings.previewMovementSpeed);

    buf->appendf("\n");
}

bool ImGuiCustomTextFilter::Draw(const char* label, float width)
{
    if (width != 0.0f)
    {
        ImGui::SetNextItemWidth(width);
    }

    const bool valChanged = ImGui::InputText(label, &inputBuf);
    if (valChanged)
    {
        Build();
    }

    return valChanged;
}

void ImGuiCustomTextFilter::TxtRange::split(const char separator, std::vector<TxtRange>* out) const
{
    assert(out->size() == 0);

    const char* wb = b;
    const char* we = wb;
    while (we < e)
    {
        if (*we == separator)
        {
            out->push_back(TxtRange(wb, we));
            wb = (we + 1);
        }
        we++;
    }

    if (wb != we)
    {
        out->push_back(TxtRange(wb, we));
    }
}

void ImGuiCustomTextFilter::Build()
{
    filters.clear();
    TxtRange txtInputRange(inputBuf.c_str(), inputBuf.end()._Ptr);
    txtInputRange.split(',', &filters);

    grepCnt = 0;
    for (TxtRange& f : filters)
    {
        while (f.b < f.e && charIsBlank(f.b[0]))
        {
            f.b++;
        }

        while (f.e > f.b && charIsBlank(f.e[-1]))
        {
            f.e--;
        }

        if (f.empty())
        {
            continue;
        }

        if (f.b[0] != '-')
        {
            grepCnt += 1;
        }
    }
}

bool ImGuiCustomTextFilter::PassFilter(const char* text, const char* text_end) const
{
    if (filters.size() == 0)
    {
        return true;
    }

    if (text == nullptr)
    {
        text = "";
        text_end = "";
    }

    for (const TxtRange& f : filters)
    {
        if (f.b == f.e)
        {
            continue;
        }

        if (f.b[0] == '-')
        {
            // exclude lookup
            if (ImStristr(text, text_end, f.b + 1, f.e) != NULL)
            {
                return false;
            }
        }
        else
        {
            // normal grep lookup
            if (ImStristr(text, text_end, f.b, f.e) != NULL)
            {
                return true;
            }
        }
    }

    // implicit * grep
    if (grepCnt == 0)
    {
        return true;
    }

    return false;
}

void ImGuiHandler::SetupHandler()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "imgui.ini";

    ImGuiSettingsHandler assetSettingsHandler = {};
    assetSettingsHandler.TypeName = "AssetSettings";
    assetSettingsHandler.TypeHash = ImHashStr("AssetSettings");
    assetSettingsHandler.ReadOpenFn = AssetSettings_ReadOpen;
    assetSettingsHandler.ReadLineFn = AssetSettings_ReadLine;
    assetSettingsHandler.WriteAllFn = AssetSettings_WriteAll;

    ImGuiSettingsHandler utilSettingsHandler = {};
    utilSettingsHandler.TypeName = "UtilSettings";
    utilSettingsHandler.TypeHash = ImHashStr("UtilSettings");
    utilSettingsHandler.ReadOpenFn = UtilSettings_ReadOpen;
    utilSettingsHandler.ReadLineFn = UtilSettings_ReadLine;
    utilSettingsHandler.WriteAllFn = UtilSettings_WriteAll;

    // [rika]: go to for adding simple saved settings
    // related to exporting assets (not auto generated format settings (AssetSettings))
    ImGuiSettingsHandler exportSettingsHandler = {};
    exportSettingsHandler.TypeName = "ExportSettings";
    exportSettingsHandler.TypeHash = ImHashStr("ExportSettings");
    exportSettingsHandler.ReadOpenFn = ExportSettings_ReadOpen;
    exportSettingsHandler.ReadLineFn = ExportSettings_ReadLine;
    exportSettingsHandler.WriteAllFn = ExportSettings_WriteAll;

    // related to preview (in the 3D viewport)
    ImGuiSettingsHandler previewSettingsHandler = {};
    previewSettingsHandler.TypeName = "PreviewSettings";
    previewSettingsHandler.TypeHash = ImHashStr("PreviewSettings");
    previewSettingsHandler.ReadOpenFn = PreviewSettings_ReadOpen;
    previewSettingsHandler.ReadLineFn = PreviewSettings_ReadLine;
    previewSettingsHandler.WriteAllFn = PreviewSettings_WriteAll;

    // theme/appearance settings
    ImGuiSettingsHandler themeSettingsHandler = {};
    themeSettingsHandler.TypeName = "ThemeSettings";
    themeSettingsHandler.TypeHash = ImHashStr("ThemeSettings");
    themeSettingsHandler.ReadOpenFn = ThemeSettings_ReadOpen;
    themeSettingsHandler.ReadLineFn = ThemeSettings_ReadLine;
    themeSettingsHandler.WriteAllFn = ThemeSettings_WriteAll;

    ImGui::AddSettingsHandler(&assetSettingsHandler);
    ImGui::AddSettingsHandler(&utilSettingsHandler);
    ImGui::AddSettingsHandler(&exportSettingsHandler);
    ImGui::AddSettingsHandler(&previewSettingsHandler);
    ImGui::AddSettingsHandler(&themeSettingsHandler);
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
}

void ImGuiHandler::SetStyle()
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;
    config.MergeMode = true;
    config.OversampleH = 2;
    config.OversampleV = 2;

    this->defaultFont = io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, sizeof(SourceSansProRegular_compressed_data), 18.f, NULL, io.Fonts->GetGlyphRangesDefault());

    char* systemRootPath = nullptr;
    size_t systemRootPathLen = 0;
    assertm(_dupenv_s(&systemRootPath, &systemRootPathLen, "SYSTEMROOT") == 0, "couldn't get systemroot env var");

    const std::string fontsDir = std::string(systemRootPath) + "\\Fonts\\";
    io.Fonts->AddFontFromFileTTF((fontsDir + "simsun.ttc").c_str(), 18.f, &config, io.Fonts->GetGlyphRangesChineseFull());
    io.Fonts->AddFontFromFileTTF((fontsDir + "malgun.ttf").c_str(), 18.f, &config, io.Fonts->GetGlyphRangesJapanese());
    io.Fonts->AddFontFromFileTTF((fontsDir + "micross.ttf").c_str(), 18.f, &config, io.Fonts->GetGlyphRangesCyrillic());

    ApplyTheme();
}

void ImGuiHandler::ApplyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    const ThemeSettings_t& t = theme;

    // Helper macro to darken a color
    #define DARKEN(c, amt) ImVec4( \
        (std::max)((c).x - (amt), 0.0f), \
        (std::max)((c).y - (amt), 0.0f), \
        (std::max)((c).z - (amt), 0.0f), \
        (c).w)

    colors[ImGuiCol_Text] = t.textRegular;
    colors[ImGuiCol_TextDisabled] = ImVec4(t.textRegular.x * 0.6f, t.textRegular.y * 0.6f, t.textRegular.z * 0.6f, t.textRegular.w * 0.6f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(t.accentColor.x * 0.3f, t.accentColor.y * 0.3f, t.accentColor.z * 0.3f, 0.5f);
    colors[ImGuiCol_WindowBg] = t.windowBg;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(t.windowBg.x * 1.1f, t.windowBg.y * 1.1f, t.windowBg.z * 1.1f, 1.0f);
    colors[ImGuiCol_Border] = DARKEN(t.accentColor, 0.3f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = DARKEN(t.windowBg, 0.1f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(t.windowBg.x - 0.05f, t.windowBg.y - 0.05f, t.windowBg.z - 0.05f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = t.accentColor;
    colors[ImGuiCol_TitleBg] = t.titleBg;
    colors[ImGuiCol_TitleBgActive] = t.accentActive;
    colors[ImGuiCol_TitleBgCollapsed] = DARKEN(t.titleBg, 0.2f);
    colors[ImGuiCol_MenuBarBg] = DARKEN(t.windowBg, 0.1f);
    colors[ImGuiCol_ScrollbarBg] = DARKEN(t.windowBg, 0.15f);
    colors[ImGuiCol_ScrollbarGrab] = t.accentColor;
    colors[ImGuiCol_ScrollbarGrabHovered] = t.accentHovered;
    colors[ImGuiCol_ScrollbarGrabActive] = t.accentActive;
    colors[ImGuiCol_CheckMark] = t.accentColor;
    colors[ImGuiCol_SliderGrab] = t.accentColor;
    colors[ImGuiCol_SliderGrabActive] = t.accentActive;
    colors[ImGuiCol_Button] = t.accentColor;
    colors[ImGuiCol_ButtonHovered] = t.accentHovered;
    colors[ImGuiCol_ButtonActive] = t.accentActive;
    colors[ImGuiCol_Header] = t.headerBg;
    colors[ImGuiCol_HeaderHovered] = t.accentHovered;
    colors[ImGuiCol_HeaderActive] = t.accentActive;
    colors[ImGuiCol_Separator] = DARKEN(t.accentColor, 0.3f);
    colors[ImGuiCol_SeparatorHovered] = t.accentHovered;
    colors[ImGuiCol_SeparatorActive] = t.accentActive;
    colors[ImGuiCol_ResizeGrip] = t.accentColor;
    colors[ImGuiCol_ResizeGripHovered] = t.accentHovered;
    colors[ImGuiCol_ResizeGripActive] = t.accentActive;
    colors[ImGuiCol_Tab] = t.headerBg;
    colors[ImGuiCol_TabHovered] = t.accentHovered;
    colors[ImGuiCol_TabActive] = t.accentActive;
    colors[ImGuiCol_TabUnfocused] = DARKEN(t.headerBg, 0.2f);
    colors[ImGuiCol_TabUnfocusedActive] = DARKEN(t.accentColor, 0.1f);
    colors[ImGuiCol_PlotLines] = t.accentColor;
    colors[ImGuiCol_PlotLinesHovered] = t.accentHovered;
    colors[ImGuiCol_PlotHistogram] = t.accentColor;
    colors[ImGuiCol_PlotHistogramHovered] = t.accentHovered;
    colors[ImGuiCol_TableHeaderBg] = DARKEN(t.windowBg, 0.15f);
    colors[ImGuiCol_TableBorderStrong] = t.accentColor;
    colors[ImGuiCol_TableBorderLight] = DARKEN(t.accentColor, 0.2f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = DARKEN(t.windowBg, 0.1f);
    colors[ImGuiCol_DragDropTarget] = t.accentColor;
    colors[ImGuiCol_NavHighlight] = t.accentColor;
    colors[ImGuiCol_NavWindowingHighlight] = t.accentColor;
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(t.windowBg.x * 0.5f, t.windowBg.y * 0.5f, t.windowBg.z * 0.5f, 0.6f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(t.windowBg.x * 0.4f, t.windowBg.y * 0.4f, t.windowBg.z * 0.4f, 0.7f);

    // Style rounding and sizes
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    style.WindowRounding = 4.0f;
    style.FrameRounding = 1.0f;
    style.ChildRounding = 1.0f;
    style.PopupRounding = 3.0f;
    style.TabRounding = 1.0f;
    style.ScrollbarRounding = 3.0f;
}

void ImGuiHandler::ShowThemeEditor()
{
    if (!theme.showThemeEditor)
        return;

    if (ImGui::Begin("Theme Editor", &theme.showThemeEditor))
    {
        ImGui::Text("Customize the appearance of RSX");
        ImGui::Separator();

        bool colorChanged = false;

        ImGui::Text("Accent Colors");
        colorChanged |= ImGui::ColorEdit4("Primary", &theme.accentColor.x);
        colorChanged |= ImGui::ColorEdit4("Hovered", &theme.accentHovered.x);
        colorChanged |= ImGui::ColorEdit4("Active", &theme.accentActive.x);

        ImGui::Separator();
        ImGui::Text("Background Colors");
        colorChanged |= ImGui::ColorEdit4("Window/Popup", &theme.windowBg.x);
        colorChanged |= ImGui::ColorEdit4("Header/Title", &theme.headerBg.x);
        colorChanged |= ImGui::ColorEdit4("Title Bar", &theme.titleBg.x);

        ImGui::Separator();
        ImGui::Text("Text");
        colorChanged |= ImGui::ColorEdit4("Regular", &theme.textRegular.x);

        if (colorChanged)
            ApplyTheme();

        ImGui::Separator();
        if (ImGui::Button("Reset to Default"))
        {
            theme.accentColor = ImVec4(0.45f, 0.30f, 0.70f, 1.00f);
            theme.accentHovered = ImVec4(0.50f, 0.35f, 0.80f, 1.00f);
            theme.accentActive = ImVec4(0.55f, 0.40f, 0.85f, 1.00f);
            theme.windowBg = ImVec4(0.12f, 0.10f, 0.15f, 1.00f);
            theme.headerBg = ImVec4(0.35f, 0.25f, 0.55f, 0.60f);
            theme.titleBg = ImVec4(0.35f, 0.25f, 0.55f, 1.00f);
            theme.textRegular = ImVec4(0.85f, 0.85f, 0.90f, 1.00f);
            ApplyTheme();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Theme changes are saved automatically");
    }
    ImGui::End();
}

void ImGuiHandler::HelpMarker(const char* const desc)
{
    assert(desc);
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

const ProgressBarEvent_t* const ImGuiHandler::AddProgressBarEvent(const char* const eventName, const uint32_t eventNum, std::atomic<uint32_t>* const remainingEvents, const bool isInverted)
{
    // Don't bother doing any of this if ImGui isn't running - it's not going to get rendered anyway
    if (noImGui)
        return nullptr;

    std::unique_lock<std::mutex> lock(eventMutex);

    if (eventNum != 0 && !pbAvailSlots.empty())
    {
        const uint8_t idx = pbAvailSlots.top();
        pbAvailSlots.pop();

        ProgressBarEvent_t* const event = &pbEvents[idx];
        event->isInverted = isInverted;
        event->eventName = eventName;
        event->eventNum = eventNum;
        event->remainingEvents = remainingEvents;
        event->eventClass = nullptr;
        event->fnRemainingEvents = nullptr;
        event->fnCancelEvents = nullptr; // These progress bar events are currently used for loading assets, which requires a bit more cleanup to cancel
                                         // So for now, only export events (using ImGuiHandler::AddProgressBarEvent as defined in imgui_utility.h) are cancellable

        event->slotIsUsed = true;
        return event;
    }
    else
    {
        return nullptr;
    }

    unreachable();
}

void ImGuiHandler::FinishProgressBarEvent(const ProgressBarEvent_t* const event)
{
    if (!event || noImGui)
        return;

    assert(event->slotIsUsed);
    std::unique_lock<std::mutex> lock(eventMutex);
    const uint8_t eventIdx = static_cast<uint8_t>(event - pbEvents);
    assert(eventIdx >= 0 || eventIdx < PB_SIZE);

    // already resets slotIsUsed to false
    memset(&pbEvents[eventIdx], 0, sizeof(ProgressBarEvent_t));
    pbAvailSlots.push(eventIdx);
}

void ImGuiHandler::HandleProgressBar()
{
    // If the app is running without ImGui being initialised, don't try and run this method
    if (noImGui)
        return;

    std::unique_lock<std::mutex> lock(eventMutex);

    bool foundTopLevelBar = false;
    for (int i = 0; i < PB_SIZE; ++i)
    {
        ProgressBarEvent_t* const event = &pbEvents[i];
        if (!event->slotIsUsed)
            continue;

        if (!foundTopLevelBar)
        {
            ImVec2 viewportCenter = ImGui::GetMainViewport()->GetWorkCenter();

            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            if (!ImGui::Begin(event->eventName, nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse))
                continue;
        }
        else
        {
            ImGui::Separator();
            ImGui::Text(event->eventName);
        }

        const uint32_t numEvents = event->eventNum;
        const uint32_t remainingEvents = event->getRemainingEvents();

        const uint32_t leftOverEvents = event->isInverted ? remainingEvents : numEvents - remainingEvents;
        const float progressFraction = std::clamp(static_cast<float>(leftOverEvents) / static_cast<float>(numEvents), 0.0f, 1.0f);
        ProgressBarCentered(progressFraction, ImVec2(485, 48), std::format("{}/{}", leftOverEvents, numEvents).c_str(), event);

        foundTopLevelBar = true;
    }

    if(foundTopLevelBar)
        ImGui::End();
}

// size_arg (for each axis) < 0.0f: align to end, 0.0f: auto, > 0.0f: specified size
void ImGuiHandler::ProgressBarCentered(float fraction, const ImVec2& size_arg, const char* overlay, ProgressBarEvent_t* event)
{
    if (g_pImGuiHandler->noImGui)
        return;

    using namespace ImGui;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    fraction = ImSaturate(fraction);
    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    const ImVec2 fill_br = ImVec2(ImLerp(bb.Min.x, bb.Max.x, fraction), bb.Max.y);
    RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), 0.0f, fraction, style.FrameRounding);

    // Default displaying the fraction as percentage string, but user can override it
    char overlay_buf[32];
    if (!overlay)
    {
        ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f%%", fraction * 100 + 0.01f);
        overlay = overlay_buf;
    }

    ImVec2 overlay_size = CalcTextSize(overlay, NULL);
    if (overlay_size.x > 0.0f)
        RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y), bb.Max, overlay, NULL, &overlay_size, ImVec2(0.5f, 0.5f), &bb);

    if (event->fnCancelEvents)
    {
        // https://github.com/ocornut/imgui/issues/4157
        float cancelButtonWidth = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.f;
        float widthNeeded = style.ItemSpacing.x + cancelButtonWidth - 5.f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - widthNeeded);

        if (ImGui::Button("Cancel"))
            event->fnCancelEvents(event);
    }
}

ImGuiHandler::ImGuiHandler()
{
    const uint32_t totalThreadCount = CThread::GetConCurrentThreads();

    // need at least one thread.
    cfg.exportThreadCount = 1u;
    cfg.parseThreadCount = std::max(totalThreadCount >> 1u, 1u);

    // standard config setting for compression
    cfg.compressionLevel = eCompressionLevel::CMPR_LVL_VERYFAST;

    memset(pbEvents, 0, sizeof(pbEvents));
    for (int8_t i = PB_SIZE - 1; i >= 0; --i) // in reverse order
    {
        pbAvailSlots.push(static_cast<int8_t>(i));
    }

    noImGui = false;
}

ImGuiHandler* g_pImGuiHandler = new ImGuiHandler();