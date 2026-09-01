#pragma once
#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <core/utils/thread.h>

#define PB_SIZE 16
#define PB_FNCLASS_TO_VOID(memberFunc) \
    [](auto ptr) { \
        return reinterpret_cast<void*&>(ptr); \
    }(memberFunc)

struct ProgressBarEvent_t
{
    typedef void(*CancelEventCallback_f)(ProgressBarEvent_t*);

    const uint32_t getRemainingEvents() const
    {
        if (fnRemainingEvents)
        {
            if (eventClass)
                return reinterpret_cast<const uint32_t(*)(void*)>(fnRemainingEvents)(eventClass);
            else
                return reinterpret_cast<const uint32_t(*)()>(fnRemainingEvents)();

        }
        else if (remainingEvents)
        {
            return *remainingEvents;
        }
        
        assert(false);
        return 1;
    }

    static void cancelEvents(ProgressBarEvent_t* event)
    {
        if (event->eventClass)
        {
            CParallelTask* task = reinterpret_cast<CParallelTask*>(event->eventClass);

            task->clear();
        }
    }

    bool slotIsUsed;
    bool isInverted;
    const char* eventName;
    uint32_t eventNum;
    std::atomic<uint32_t>* remainingEvents;
    void* eventClass;
    void* fnRemainingEvents;
    CancelEventCallback_f fnCancelEvents;
};

class ImGuiCustomTextFilter
{
public:

    ImGuiCustomTextFilter()
    {
        inputBuf.clear();

        grepCnt = 0;
        Build();
    }

    bool Draw(const char* label = "Filter", float width = 0.0f);
    bool PassFilter(const char* text, const char* textEnd = nullptr) const;
    void Build();

    void Clear()
    {
        inputBuf.clear();
        Build();
    };

    bool IsActive() const
    {
        return !filters.empty();
    };

    inline char charToUpper(const char c)
    {
        return (c >= 'a' && c <= 'z') ? (c & ~0x20) : c;
    }

    inline bool charIsBlank(const char c)
    {
        return c == ' ' || c == '\t';
    }

    struct TxtRange
    {
        const char* b;
        const char* e;

        TxtRange()
        {
            b = nullptr;
            e = nullptr;
        }

        TxtRange(const char* b, const char* e) : b(b), e(e)
        {
        };

        bool empty() const
        { 
            return b == e;
        }

        void split(char separator, std::vector<TxtRange>* out) const;
    };

    std::string inputBuf;
    std::vector<TxtRange> filters;
    int grepCnt;
};

class ImGuiHandler
{
public:
    ImGuiHandler();
    void SetupHandler();
    void SetStyle();

    void SetNoImGui(bool state) { noImGui = state; };

    void HelpMarker(const char* const desc);

    template <typename T>
    const ProgressBarEvent_t* const AddProgressBarEvent(const char* const eventName, const uint32_t eventNum, T const eventClass, void* const fnRemainingEvents, ProgressBarEvent_t::CancelEventCallback_f fnCancelEvents=ProgressBarEvent_t::cancelEvents)
    {
        std::unique_lock<std::mutex> lock(eventMutex);
        if (eventNum != 0 && !pbAvailSlots.empty())
        {
            const uint8_t idx = pbAvailSlots.top();
            pbAvailSlots.pop();

            ProgressBarEvent_t* const event = &pbEvents[idx];
            event->isInverted = false;
            event->eventName = eventName;
            event->eventNum = eventNum;
            event->remainingEvents = nullptr;
            event->eventClass = reinterpret_cast<void*>(eventClass);
            event->fnRemainingEvents = fnRemainingEvents;
            event->fnCancelEvents = fnCancelEvents;

            event->slotIsUsed = true;
            return event;
        }
        else
        {
            return nullptr;
        }

        unreachable();
    }

    const ProgressBarEvent_t* const AddProgressBarEvent(const char* const eventName, const uint32_t eventNum, std::atomic<uint32_t>* const remainingEvents, const bool isInverted);
    void FinishProgressBarEvent(const ProgressBarEvent_t* const event);
    void HandleProgressBar();

    struct UtilsSettings_t
    {
        uint32_t parseThreadCount;
        uint32_t exportThreadCount;
        uint32_t compressionLevel;
        bool checkForUpdatesOnStartup = false;
    } cfg;

    struct FilterSettings_t
    {
        ImGuiCustomTextFilter textFilter;
    } filter;

    struct ThemeSettings_t
    {
        ImVec4 accentColor = ImVec4(0.45f, 0.30f, 0.70f, 1.00f);
        ImVec4 accentHovered = ImVec4(0.50f, 0.35f, 0.80f, 1.00f);
        ImVec4 accentActive = ImVec4(0.55f, 0.40f, 0.85f, 1.00f);
        ImVec4 backgroundColor = ImVec4(0.12f, 0.10f, 0.15f, 1.00f);
        ImVec4 windowBg = ImVec4(0.12f, 0.10f, 0.15f, 1.00f);
        ImVec4 textRegular = ImVec4(0.85f, 0.85f, 0.90f, 1.00f);
        ImVec4 headerBg = ImVec4(0.35f, 0.25f, 0.55f, 0.60f);
        ImVec4 titleBg = ImVec4(0.35f, 0.25f, 0.55f, 1.00f);
        bool showThemeEditor = false;
    } theme;

    ImFont* GetDefaultFont() const { return defaultFont; };
    ImFont* GetMonospaceFont() const { return monospaceFont; };

    // custom ImGui widgets

    static void ProgressBarCentered(float fraction, const ImVec2& size_arg, const char* overlay, ProgressBarEvent_t* event);

    void ShowThemeEditor();
    void ApplyTheme();

private:
    std::mutex eventMutex;
    ProgressBarEvent_t pbEvents[PB_SIZE];
    std::stack<uint8_t> pbAvailSlots;

    ImFont* defaultFont;
    ImFont* monospaceFont;

    bool noImGui;
};

extern ImGuiHandler* g_pImGuiHandler;

#define UtilsConfig (&g_pImGuiHandler->cfg)
#define FilterConfig (&g_pImGuiHandler->filter)

// https://github.com/libigl/libigl/issues/1300#issuecomment-1310174619
static std::string _labelPrefix(const char* const label, int inputRelPosX)
{
    //float width = ImGui::CalcItemWidth();

    const float x = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::Text(label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + inputRelPosX + ImGui::GetStyle().ItemInnerSpacing.x);

    std::string labelID = "##";
    labelID += label;

    ImGui::SetNextItemWidth(-1);

    return labelID;
}
inline void ImGuiConstTextInputLeft(const char* label, const char* text, int inputRelPosX = 170)
{
    const std::string lblText = _labelPrefix(label, inputRelPosX);

    ImGui::InputText(lblText.c_str(), const_cast<char*>(text), strlen(text), ImGuiInputTextFlags_ReadOnly);
}

inline void ImGuiConstIntInputLeft(const char* label, const int val, int inputRelPosX = 170, ImGuiInputTextFlags flags = 0)
{
    const std::string lblText = _labelPrefix(label, inputRelPosX);

    ImGui::InputInt(label, const_cast<int*>(&val), 0, 0, ImGuiInputTextFlags_ReadOnly | flags);
}