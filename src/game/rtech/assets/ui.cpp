#include "pch.h"
#include "ui.h"

#include <iomanip>
#include <algorithm>
#include <set>

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>

extern ExportSettings_t g_ExportSettings;

//-----------------------------------------------------------------------------
// Memory safety helpers for V39+ assets where pointers may be unaligned
// or memory may be freed between load and export
//-----------------------------------------------------------------------------
static bool UI_IsMemoryReadable(const void* ptr, size_t size)
{
    if (!ptr || size == 0)
        return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0)
        return false;

    bool readable = (mbi.State == MEM_COMMIT) &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
                                    PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
                    !(mbi.Protect & PAGE_GUARD);

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    if (readable && (addr + size > regionBase + mbi.RegionSize))
        return false;

    return readable;
}

static bool SafeReadUIArg(const UIAssetArg_t* arg, uint8_t* outType, uint8_t* outUnk1, uint16_t* outDataOffset, uint16_t* outNameOffset, uint16_t* outShortHash)
{
    __try
    {
        *outType = arg->type;
        *outUnk1 = arg->unk_1;
        *outDataOffset = arg->dataOffset;
        *outNameOffset = arg->nameOffset;
        *outShortHash = arg->shortHash;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool SafeReadFloat(const void* ptr, float* out)
{
    __try { memcpy(out, ptr, sizeof(float)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { *out = 0.0f; return false; }
}

static bool SafeReadFloat2(const void* ptr, float* out)
{
    __try { memcpy(out, ptr, sizeof(float) * 2); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { out[0] = out[1] = 0.0f; return false; }
}

static bool SafeReadFloat3(const void* ptr, float* out)
{
    __try { memcpy(out, ptr, sizeof(float) * 3); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { out[0] = out[1] = out[2] = 0.0f; return false; }
}

static bool SafeReadFloat4(const void* ptr, float* out)
{
    __try { memcpy(out, ptr, sizeof(float) * 4); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { out[0] = out[1] = out[2] = out[3] = 0.0f; return false; }
}

static bool SafeReadInt(const void* ptr, int* out)
{
    __try { memcpy(out, ptr, sizeof(int)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { *out = 0; return false; }
}

static bool SafeReadInt64(const void* ptr, int64_t* out)
{
    __try { memcpy(out, ptr, sizeof(int64_t)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { *out = 0; return false; }
}

static bool SafeReadBool(const void* ptr, bool* out)
{
    __try { memcpy(out, ptr, sizeof(bool)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { *out = false; return false; }
}

static bool IsValidReadablePointer(const void* ptr)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    // Reject NULL, low addresses, kernel space, and common sentinel values
    if (addr == 0 || addr < 0x10000 || addr > 0x00007FFFFFFFFFFF)
        return false;
    if (addr == 0xFFFFFFFF || addr == 0xFFFFFFFFFFFFFFFF)
        return false;
    if (addr == 0xCDCDCDCDCDCDCDCD || addr == 0xDDDDDDDDDDDDDDDD || addr == 0xFEEEFEEEFEEEFEEE)
        return false;
    // Must be above 1MB to be a realistic heap/data pointer
    if (addr < 0x100000)
        return false;
    return true;
}

static bool SafeCopyString(const char* src, char* dest, size_t maxLen)
{
    if (!src || !dest || maxLen == 0)
        return false;

    if (!IsValidReadablePointer(src))
        return false;

    if (!UI_IsMemoryReadable(src, 1))
        return false;

    size_t i = 0;
    for (; i < maxLen - 1 && src[i] != '\0'; i++)
        dest[i] = src[i];
    dest[i] = '\0';
    return i > 0;
}

static bool LooksLikeInlineString(const void* ptr, size_t maxCheck = 8)
{
    if (!ptr) return false;
    __try
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
        int printableCount = 0;
        for (size_t i = 0; i < maxCheck; i++)
        {
            uint8_t b = bytes[i];
            if (b == 0) return printableCount >= 1;
            if ((b >= 0x20 && b <= 0x7E) || b == '\t' || b == '\n' || b == '\r')
                printableCount++;
            else
                return false;
        }
        return printableCount >= 2;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool LooksLikeValidPointer(const void* ptr)
{
    if (!ptr) return false;
    uintptr_t val = 0;
    memcpy(&val, ptr, sizeof(uintptr_t));
    return IsValidReadablePointer(reinterpret_cast<const void*>(val));
}

// Dereference a pointer stored at valuePtr and read the string it points to
static bool SafeReadStringPointerDirect(const void* valuePtr, char* outBuffer, size_t bufferSize)
{
    if (!valuePtr || !outBuffer || bufferSize == 0) return false;
    outBuffer[0] = '\0';

    if (!IsValidReadablePointer(valuePtr)) return false;

    const char* strPtr = nullptr;
    memcpy(&strPtr, valuePtr, sizeof(const char*));

    if (!IsValidReadablePointer(strPtr)) return false;

    return SafeCopyString(strPtr, outBuffer, bufferSize);
}

static size_t GetUICpuDataSize(CPakAsset* pakAsset)
{
    const PakAsset_t* pakData = pakAsset->data();
    if (!pakData->HasValidDataPagePointer())
        return 0;

    __try
    {
        CPakFile* pakFile = pakAsset->GetContainerFile<CPakFile>();
        if (!pakFile || !pakFile->header())
            return 0;

        const int startPageIndex = pakData->dataPagePtr.index;
        const int pageOffset = pakData->dataPagePtr.offset;
        const int endPageIndex = pakData->pageEnd;
        const int numPages = pakFile->header()->numPages;

        if (startPageIndex < 0 || startPageIndex >= numPages || numPages <= 0 || numPages > 10000)
            return 0;

        const PakPageHdr_t* pageHeaders = pakFile->header()->GetPageHeaders();
        if (!pageHeaders)
            return 0;

        size_t totalSize = 0;
        int actualEndPage = (endPageIndex > startPageIndex && endPageIndex <= numPages) ? endPageIndex : startPageIndex + 1;

        for (int i = startPageIndex; i < actualEndPage; i++)
        {
            const PakPageHdr_t& page = pageHeaders[i];
            if (page.size == 0 || page.size > 100 * 1024 * 1024)
                continue;

            if (i == startPageIndex)
            {
                if (pageOffset >= 0 && pageOffset < (int)page.size)
                    totalSize += page.size - pageOffset;
            }
            else
                totalSize += page.size;
        }

        if (totalSize > 1024 * 1024)
            totalSize = 1024 * 1024;

        return totalSize;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// Read string value handling both pointer-based and inline formats
static bool ReadStringValue(const void* dataPtr, char* outBuffer, size_t bufferSize, [[maybe_unused]] int assetVersion)
{
    if (!dataPtr || !outBuffer || bufferSize == 0) return false;
    outBuffer[0] = '\0';

    __try
    {
        if (LooksLikeInlineString(dataPtr))
            return SafeCopyString(static_cast<const char*>(dataPtr), outBuffer, bufferSize);

        if (LooksLikeValidPointer(dataPtr))
        {
            const char* strPtr = nullptr;
            memcpy(&strPtr, dataPtr, sizeof(const char*));
            if (strPtr)
                return SafeCopyString(strPtr, outBuffer, bufferSize);
            return false;
        }

        // Fallback: try as pointer first
        const char* strPtr = nullptr;
        memcpy(&strPtr, dataPtr, sizeof(const char*));
        if (strPtr && SafeCopyString(strPtr, outBuffer, bufferSize) && outBuffer[0] != '\0')
            return true;

        // Last resort: try as inline string
        return SafeCopyString(static_cast<const char*>(dataPtr), outBuffer, bufferSize);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

//-----------------------------------------------------------------------------
// Load / PostLoad
//-----------------------------------------------------------------------------
void LoadUIAsset(CAssetContainer* const container, CAsset* const asset)
{
    UNUSED(container);

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    UIAsset* uiAsset = nullptr;

    switch (pakAsset->version())
    {
    case 29:
    case 30:
    case 39:
    case 40:
    case 42:
    {
        UIAssetHeader_t* const hdr = reinterpret_cast<UIAssetHeader_t* const>(pakAsset->header());
        uiAsset = new UIAsset(hdr, pakAsset->version());
        break;
    }
    default:
    {
        assertm(false, "unaccounted asset version, will cause major issues!");
        return;
    }
    }

    if (uiAsset->GetName())
    {
        const std::string uiName = "ui/" + std::string(uiAsset->GetName()) + ".rpak";
        pakAsset->SetAssetName(uiName, true);
    }
    else
        pakAsset->SetAssetNameFromCache();

    pakAsset->setExtraData(uiAsset);
}

// SEH-isolated string reader: no C++ objects in scope, safe for Release /O2
// Returns true if a string was read into outBuffer
static bool SEH_ReadArgString(void* defaultValues, uint16_t dataOffset, uint16_t defaultValuesSize, int version, char* outBuffer, size_t bufferSize)
{
    outBuffer[0] = '\0';

    if (!defaultValues || dataOffset >= defaultValuesSize)
        return false;

    void* valuePtr = reinterpret_cast<char*>(defaultValues) + dataOffset;

    if (SafeReadStringPointerDirect(valuePtr, outBuffer, bufferSize))
        return true;

    if (ReadStringValue(valuePtr, outBuffer, bufferSize, version))
        return true;

    return false;
}

void PostLoadUIAsset(CAssetContainer* const container, CAsset* const asset)
{
    UNUSED(container);

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    if (!pakAsset->extraData())
        return;

    UIAsset* const uiAsset = reinterpret_cast<UIAsset*>(pakAsset->extraData());

    if (!uiAsset->GetName())
        pakAsset->SetAssetNameFromCache();

    // Cache string values while memory is still accessible
    // SEH reads are isolated in SEH_ReadArgString (no C++ objects in SEH scope)
    if (uiAsset->GetArgs() && uiAsset->GetDefaultValues() && uiAsset->argCount > 0)
    {
        for (int16_t i = 0; i < uiAsset->argCount; i++)
        {
            uint8_t argType = uiAsset->GetArgs()[i].type;

            if (argType != UI_ARG_TYPE_STRING && argType != UI_ARG_TYPE_ASSET &&
                argType != UI_ARG_TYPE_IMAGE && argType != UI_ARG_TYPE_UIHANDLE &&
                argType != UI_ARG_TYPE_FONT_FACE)
                continue;

            uint16_t dataOffset = uiAsset->GetArgs()[i].dataOffset;
            char strBuf[1024] = {0};

            if (SEH_ReadArgString(uiAsset->GetDefaultValues(), dataOffset, uiAsset->argDefaultValueSize, pakAsset->version(), strBuf, sizeof(strBuf)))
                uiAsset->CacheString(dataOffset, strBuf);
            else
                uiAsset->CacheString(dataOffset, "");
        }
    }
}

//-----------------------------------------------------------------------------
// Preview
//-----------------------------------------------------------------------------
struct UIPreviewData_t
{
    enum eColumnID
    {
        TPC_Index,
        TPC_Type,
        TPC_Name,
        TPC_DefaultVal,
        TPC_Offset,

        _TPC_COUNT,
    };

    const char* name;
    UIAssetArgValue_t value;
    const char* typeStr;
    int index;
    uint16_t hash;
    UIAssetArgType_t type;
    uint16_t offset;

    const bool operator==(const UIPreviewData_t& in)
    {
        return name == in.name && value.rawptr == in.value.rawptr && type == in.type;
    }

    const bool operator==(const UIPreviewData_t* in)
    {
        return name == in->name && value.rawptr == in->value.rawptr && type == in->type;
    }
};

struct UICompare_t
{
    const ImGuiTableSortSpecs* sortSpecs;

    bool operator() (const UIPreviewData_t& a, const UIPreviewData_t& b) const
    {
        for (int n = 0; n < sortSpecs->SpecsCount; n++)
        {
            const ImGuiTableColumnSortSpecs* sort_spec = &sortSpecs->Specs[n];
            __int64 delta = 0;
            switch (sort_spec->ColumnUserID)
            {
            case UIPreviewData_t::eColumnID::TPC_Index:     delta = (a.index - b.index);                                                break;
            case UIPreviewData_t::eColumnID::TPC_Type:      delta = (static_cast<uint8_t>(a.type) - static_cast<uint8_t>(b.type));      break;
            case UIPreviewData_t::eColumnID::TPC_Offset:    delta = (a.offset - b.offset);                                              break;
            }
            if (delta > 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? false : true;
            if (delta < 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? true : false;
        }

        return (a.index - b.index) > 0;
    }
};

void* PreviewUIAsset(CAsset* const asset, const bool firstFrameForAsset)
{
    assertm(asset, "Asset should be valid.");

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    const int version = pakAsset->version();
    if (version != 29 && version != 30 && version != 39 && version != 40 && version != 42)
    {
        ImGui::Text("This asset version (%d) is not currently supported for preview.", version);
        return nullptr;
    }

    UIAsset* const uiAsset = reinterpret_cast<UIAsset*>(pakAsset->extraData());
    if (!uiAsset)
    {
        ImGui::TextUnformatted("UI asset data not available.");
        return nullptr;
    }

    static std::vector<UIPreviewData_t> previewData;

    if (!uiAsset->GetArgs() || uiAsset->argCount <= 0)
    {
        ImGui::TextUnformatted("No arguments in this UI asset.");
        return nullptr;
    }

    if (firstFrameForAsset)
    {
        previewData.clear();
        previewData.resize(uiAsset->argCount);

        for (int i = 0; i < uiAsset->argCount; i++)
        {
            UIPreviewData_t& argPreviewData = previewData.at(i);
            const UIAssetArg_t* const argData = &uiAsset->GetArgs()[i];

            uint8_t argType = 0;
            uint8_t argUnk1 = 0;
            uint16_t dataOffset = 0;
            uint16_t nameOffset = 0;
            uint16_t shortHash = 0;

            if (!SafeReadUIArg(argData, &argType, &argUnk1, &dataOffset, &nameOffset, &shortHash))
            {
                argPreviewData.index = i;
                argPreviewData.name = nullptr;
                argPreviewData.value.rawptr = nullptr;
                argPreviewData.type = UIAssetArgType_t::UI_ARG_TYPE_NONE;
                argPreviewData.typeStr = s_UIArgTypeNames[0];
                argPreviewData.offset = 0;
                argPreviewData.hash = 0;
                continue;
            }

            if (argType >= 16)
                argType = 0;

            if (dataOffset >= uiAsset->argDefaultValueSize && static_cast<UIAssetArgType_t>(argType) != UIAssetArgType_t::UI_ARG_TYPE_NONE)
            {
                argPreviewData.index = i;
                argPreviewData.name = nullptr;
                argPreviewData.value.rawptr = nullptr;
                argPreviewData.type = static_cast<UIAssetArgType_t>(argType);
                argPreviewData.typeStr = s_UIArgTypeNames[argType];
                argPreviewData.offset = dataOffset;
                argPreviewData.hash = shortHash;
                continue;
            }

            argPreviewData.index = i;
            argPreviewData.name = uiAsset->GetArgNames() ? uiAsset->GetArgNames() + nameOffset : nullptr;
            argPreviewData.value.rawptr = uiAsset->GetDefaultValues() ? (reinterpret_cast<char*>(uiAsset->GetDefaultValues()) + dataOffset) : nullptr;
            argPreviewData.type = static_cast<UIAssetArgType_t>(argType);
            argPreviewData.typeStr = s_UIArgTypeNames[argType];
            argPreviewData.offset = dataOffset;
            argPreviewData.hash = shortHash;
        }
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    const ImVec2 outerSize = ImVec2(0.f, ImGui::GetTextLineHeightWithSpacing() * 12.f);

    if (ImGui::BeginTable("Arg Table", UIPreviewData_t::eColumnID::_TPC_COUNT, tableFlags, outerSize))
    {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.0f, UIPreviewData_t::eColumnID::TPC_Index);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.0f, UIPreviewData_t::eColumnID::TPC_Type);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.0f, UIPreviewData_t::eColumnID::TPC_Name);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.0f, UIPreviewData_t::eColumnID::TPC_DefaultVal);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.0f, UIPreviewData_t::eColumnID::TPC_Offset);
        ImGui::TableSetupScrollFreeze(1, 1);

        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();

        if (sortSpecs && (firstFrameForAsset || sortSpecs->SpecsDirty) && previewData.size() > 1)
        {
            std::sort(previewData.begin(), previewData.end(), UICompare_t(sortSpecs));
            sortSpecs->SpecsDirty = false;
        }

        ImGui::TableHeadersRow();

        for (int i = 0; i < uiAsset->argCount; i++)
        {
            const UIPreviewData_t* item = &previewData.at(i);

            ImGui::PushID(item->index);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);

            if (ImGui::TableSetColumnIndex(UIPreviewData_t::eColumnID::TPC_Index))
                ImGui::Text("%i", item->index);

            if (ImGui::TableSetColumnIndex(UIPreviewData_t::eColumnID::TPC_Type))
                ImGui::Text("%s", item->typeStr ? item->typeStr : "UNKNOWN");

            if (item->type == UIAssetArgType_t::UI_ARG_TYPE_NONE)
            {
                ImGui::PopID();
                continue;
            }

            if(ImGui::TableSetColumnIndex(UIPreviewData_t::eColumnID::TPC_Offset))
                ImGui::Text("0x%X", item->offset);

            if (ImGui::TableSetColumnIndex(UIPreviewData_t::eColumnID::TPC_Name))
            {
                if (item->name)
                {
                    char safeName[256] = {0};
                    if (SafeCopyString(item->name, safeName, sizeof(safeName)))
                        ImGui::Text("%s", safeName);
                    else
                        ImGui::Text("%x", item->hash);
                }
                else
                    ImGui::Text("%x", item->hash);
            }

            if (ImGui::TableSetColumnIndex(UIPreviewData_t::eColumnID::TPC_DefaultVal))
            {
                if (!item->value.rawptr)
                {
                    ImGui::TextUnformatted("(null)");
                }
                else
                {
                    switch (item->type)
                    {
                    case UIAssetArgType_t::UI_ARG_TYPE_NONE: break;
                    case UIAssetArgType_t::UI_ARG_TYPE_STRING:
                    {
                        const char* cached = uiAsset->GetCachedString(item->offset);
                        ImGui::Text("\"%s\"", (cached && cached[0]) ? cached : "");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_ASSET:
                    case UIAssetArgType_t::UI_ARG_TYPE_IMAGE:
                    case UIAssetArgType_t::UI_ARG_TYPE_UIHANDLE:
                    case UIAssetArgType_t::UI_ARG_TYPE_FONT_FACE:
                    {
                        const char* cached = uiAsset->GetCachedString(item->offset);
                        ImGui::Text("$\"%s\"", (cached && cached[0]) ? cached : "");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_BOOL:
                    {
                        bool boolVal = false;
                        if (SafeReadBool(item->value.boolean, &boolVal))
                            ImGui::Text("%s", boolVal ? "True" : "False");
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_INT:
                    {
                        int intVal = 0;
                        if (SafeReadInt(item->value.integer, &intVal))
                            ImGui::Text("%i", intVal);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_FLOAT:
                    case UIAssetArgType_t::UI_ARG_TYPE_GAMETIME:
                    {
                        float floatVal = 0.0f;
                        if (SafeReadFloat(item->value.fpn, &floatVal))
                            ImGui::Text("%f", floatVal);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_FLOAT2:
                    {
                        float floatVals[2] = {0.0f, 0.0f};
                        if (SafeReadFloat2(item->value.fpn, floatVals))
                            ImGui::Text("%f, %f", floatVals[0], floatVals[1]);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_FLOAT3:
                    {
                        float floatVals[3] = {0.0f, 0.0f, 0.0f};
                        if (SafeReadFloat3(item->value.fpn, floatVals))
                            ImGui::Text("%f, %f, %f", floatVals[0], floatVals[1], floatVals[2]);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_COLOR_ALPHA:
                    {
                        float floatVals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        if (SafeReadFloat4(item->value.fpn, floatVals))
                            ImGui::Text("%f, %f, %f, %f", floatVals[0], floatVals[1], floatVals[2], floatVals[3]);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    case UIAssetArgType_t::UI_ARG_TYPE_WALLTIME:
                    {
                        int64_t intVal = 0;
                        if (SafeReadInt64(item->value.integer64, &intVal))
                            ImGui::Text("%lli", intVal);
                        else
                            ImGui::TextUnformatted("-");
                        break;
                    }
                    default:
                        ImGui::TextUnformatted("-");
                        break;
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    return nullptr;
}

//-----------------------------------------------------------------------------
// Export
//-----------------------------------------------------------------------------
enum eUIExportSetting
{
    JSON,
};

static const char* const s_PathPrefixUI = s_AssetTypePaths.find(AssetType_t::UI)->second;
bool ExportUIAsset(CAsset* const asset, const int setting)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    UIAsset* const uiAsset = reinterpret_cast<UIAsset*>(pakAsset->extraData());

    if (!uiAsset)
        return false;

    assertm(uiAsset, "Extra data should be valid at this point.");

    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
    const std::filesystem::path uiPath(asset->GetAssetName());

    if (g_ExportSettings.exportPathsFull)
        exportPath.append(uiPath.parent_path().string());
    else
        exportPath.append(s_PathPrefixUI);

    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset type directory.");
        return false;
    }

    exportPath.append(uiPath.stem().string());

    switch (setting)
    {
    case eUIExportSetting::JSON:
    {
        std::stringstream json;
        json << "{\n";

        // Asset metadata
        json << "  \"name\": \"" << (uiAsset->GetName() ? uiAsset->GetName() : "") << "\",\n";
        json << "  \"version\": " << pakAsset->version() << ",\n";
        json << "  \"guid\": \"0x" << std::hex << std::uppercase << pakAsset->guid() << std::dec << "\",\n";
        json << std::setprecision(9);
        json << "  \"elementWidth\": " << uiAsset->elementWidth << ",\n";
        json << "  \"elementHeight\": " << uiAsset->elementHeight << ",\n";
        json << "  \"elementWidthRatio\": " << uiAsset->elementWidthRatio << ",\n";
        json << "  \"elementHeightRatio\": " << uiAsset->elementHeightRatio << ",\n";
        json << "  \"elementWidthRatioHex\": \"" << std::hex << *reinterpret_cast<const uint32_t*>(&uiAsset->elementWidthRatio) << std::dec << "\",\n";
        json << "  \"elementHeightRatioHex\": \"" << std::hex << *reinterpret_cast<const uint32_t*>(&uiAsset->elementHeightRatio) << std::dec << "\",\n";
        json << std::setprecision(6);
        json << "  \"argCount\": " << uiAsset->argCount << ",\n";
        json << "  \"argClusterCount\": " << uiAsset->argClusterCount << ",\n";
        json << "  \"defaultValuesSize\": " << uiAsset->argDefaultValueSize << ",\n";
        json << "  \"hasArgNames\": " << (uiAsset->GetArgNames() ? "true" : "false") << ",\n";

        // Args array
        json << "  \"args\": [\n";
        for (int i = 0; i < uiAsset->argCount; i++)
        {
            uint8_t argType = 0;
            uint8_t argUnk1 = 0;
            uint16_t dataOffset = 0, nameOffset = 0, shortHash = 0;

            if (SafeReadUIArg(&uiAsset->GetArgs()[i], &argType, &argUnk1, &dataOffset, &nameOffset, &shortHash))
            {
                json << "    {\n";
                json << "      \"index\": " << i << ",\n";
                json << "      \"type\": " << (int)argType << ",\n";
                json << "      \"unk_1\": " << (int)argUnk1 << ",\n";
                json << "      \"typeName\": \"" << (argType < 16 ? s_UIArgTypeNames[argType] : "UNKNOWN") << "\",\n";
                json << "      \"dataOffset\": " << dataOffset << ",\n";
                json << "      \"nameOffset\": " << nameOffset << ",\n";
                json << "      \"shortHash\": \"0x" << std::hex << shortHash << std::dec << "\"";

                if (uiAsset->GetArgNames() && nameOffset > 0)
                {
                    char nameBuf[256] = {0};
                    if (SafeCopyString(uiAsset->GetArgNames() + nameOffset, nameBuf, sizeof(nameBuf)))
                        json << ",\n      \"name\": \"" << nameBuf << "\"";
                }

                // Resolved value for all arg types
                if (uiAsset->GetDefaultValues() && dataOffset < uiAsset->argDefaultValueSize)
                {
                    void* valuePtr = reinterpret_cast<char*>(uiAsset->GetDefaultValues()) + dataOffset;
                    UIAssetArgType_t argTypeEnum = static_cast<UIAssetArgType_t>(argType);

                    if (argTypeEnum == UI_ARG_TYPE_STRING || argTypeEnum == UI_ARG_TYPE_ASSET ||
                        argTypeEnum == UI_ARG_TYPE_IMAGE || argTypeEnum == UI_ARG_TYPE_UIHANDLE ||
                        argTypeEnum == UI_ARG_TYPE_FONT_FACE)
                    {
                        const char* cachedStr = uiAsset->GetCachedString(dataOffset);
                        bool gotString = false;
                        const char* strValue = nullptr;
                        char strBuf[1024] = {0};

                        if (cachedStr)
                        {
                            strValue = cachedStr;
                            gotString = (cachedStr[0] != '\0');
                        }
                        else
                        {
                            gotString = SafeReadStringPointerDirect(valuePtr, strBuf, sizeof(strBuf));
                            if (!gotString)
                                gotString = ReadStringValue(valuePtr, strBuf, sizeof(strBuf), pakAsset->version());
                            if (gotString)
                                strValue = strBuf;
                        }

                        std::string escaped;
                        if (gotString && strValue && strValue[0] != '\0')
                        {
                            for (const char* p = strValue; *p; p++)
                            {
                                switch (*p)
                                {
                                case '\"': escaped += "\\\""; break;
                                case '\\': escaped += "\\\\"; break;
                                case '\n': escaped += "\\n"; break;
                                case '\r': escaped += "\\r"; break;
                                case '\t': escaped += "\\t"; break;
                                default:
                                    if (*p >= 0x20 && *p <= 0x7E)
                                        escaped += *p;
                                    break;
                                }
                            }
                        }
                        json << ",\n      \"stringValue\": \"" << escaped << "\"";
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_BOOL)
                    {
                        bool val = false;
                        if (SafeReadBool(valuePtr, &val))
                            json << ",\n      \"value\": " << (val ? "true" : "false");
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_INT)
                    {
                        int val = 0;
                        if (SafeReadInt(valuePtr, &val))
                            json << ",\n      \"value\": " << val;
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_FLOAT || argTypeEnum == UI_ARG_TYPE_GAMETIME)
                    {
                        float val = 0.0f;
                        if (SafeReadFloat(valuePtr, &val))
                            json << ",\n      \"value\": " << val;
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_FLOAT2)
                    {
                        float vals[2] = {0};
                        if (SafeReadFloat2(valuePtr, vals))
                            json << ",\n      \"value\": [" << vals[0] << ", " << vals[1] << "]";
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_FLOAT3)
                    {
                        float vals[3] = {0};
                        if (SafeReadFloat3(valuePtr, vals))
                            json << ",\n      \"value\": [" << vals[0] << ", " << vals[1] << ", " << vals[2] << "]";
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_COLOR_ALPHA)
                    {
                        float vals[4] = {0};
                        if (SafeReadFloat4(valuePtr, vals))
                            json << ",\n      \"value\": [" << vals[0] << ", " << vals[1] << ", " << vals[2] << ", " << vals[3] << "]";
                    }
                    else if (argTypeEnum == UI_ARG_TYPE_WALLTIME)
                    {
                        int64_t val = 0;
                        if (SafeReadInt64(valuePtr, &val))
                            json << ",\n      \"value\": " << val;
                    }
                }

                json << "\n    }";
                if (i < uiAsset->argCount - 1)
                    json << ",";
                json << "\n";
            }
        }
        json << "  ],\n";

        // DefaultValues with string sanitization
        std::vector<uint8_t> defaultStrings;
        std::vector<uint16_t> untrackedStringOffsets;
        json << "  \"defaultValuesHex\": \"";
        if (uiAsset->GetDefaultValues() && uiAsset->argDefaultValueSize > 0)
        {
            std::vector<uint8_t> sanitizedData(uiAsset->argDefaultValueSize);
            memcpy(sanitizedData.data(), uiAsset->GetDefaultValues(), uiAsset->argDefaultValueSize);

            // Replace string pointers with offsets into defaultStrings
            for (int16_t argIdx = 0; argIdx < uiAsset->argCount; argIdx++)
            {
                uint8_t argType = 0, argUnk1 = 0;
                uint16_t dataOffset = 0, nameOffset = 0, shortHash = 0;
                if (SafeReadUIArg(uiAsset->GetArgs() + argIdx, &argType, &argUnk1, &dataOffset, &nameOffset, &shortHash))
                {
                    if (argType == UI_ARG_TYPE_STRING || argType == UI_ARG_TYPE_ASSET ||
                        argType == UI_ARG_TYPE_IMAGE || argType == UI_ARG_TYPE_FONT_FACE)
                    {
                        if (dataOffset + 8 <= uiAsset->argDefaultValueSize)
                        {
                            const char* cachedStr = uiAsset->GetCachedString(dataOffset);
                            bool gotString = false;
                            const char* strValue = nullptr;
                            char strBuf[4096] = {0};

                            if (cachedStr)
                            {
                                strValue = cachedStr;
                                gotString = (cachedStr[0] != '\0');
                            }
                            else
                            {
                                uint8_t* valuePtr = reinterpret_cast<uint8_t*>(uiAsset->GetDefaultValues()) + dataOffset;
                                gotString = ReadStringValue(valuePtr, strBuf, sizeof(strBuf), pakAsset->version());
                                strValue = strBuf;
                            }

                            uint32_t stringOffset = (uint32_t)defaultStrings.size();

                            if (gotString && strValue && strValue[0] != '\0')
                            {
                                size_t len = strlen(strValue);
                                defaultStrings.insert(defaultStrings.end(), strValue, strValue + len + 1);
                            }
                            else
                                defaultStrings.push_back(0);

                            memcpy(&sanitizedData[dataOffset], &stringOffset, 4);
                            memset(&sanitizedData[dataOffset + 4], 0, 4);
                        }
                    }
                }
            }

            // Scan for untracked string pointers not associated with any argument
            {
                std::set<uint16_t> handledOffsets;
                for (int16_t argIdx = 0; argIdx < uiAsset->argCount; argIdx++)
                {
                    uint8_t argType = 0, argUnk1 = 0;
                    uint16_t dataOffset = 0, nameOffset = 0, shortHash = 0;
                    if (SafeReadUIArg(uiAsset->GetArgs() + argIdx, &argType, &argUnk1, &dataOffset, &nameOffset, &shortHash))
                    {
                        if (argType == UI_ARG_TYPE_STRING || argType == UI_ARG_TYPE_ASSET ||
                            argType == UI_ARG_TYPE_IMAGE || argType == UI_ARG_TYPE_FONT_FACE ||
                            argType == UI_ARG_TYPE_UIHANDLE)
                        {
                            handledOffsets.insert(dataOffset);
                        }
                    }
                }

                for (size_t offset = 0; offset + 8 <= uiAsset->argDefaultValueSize; offset += 8)
                {
                    if (handledOffsets.count(static_cast<uint16_t>(offset)) > 0)
                        continue;

                    const uint8_t* originalPtr = reinterpret_cast<const uint8_t*>(uiAsset->GetDefaultValues()) + offset;
                    if (!UI_IsMemoryReadable(originalPtr, 8))
                        continue;

                    uint64_t rawVal = 0;
                    memcpy(&rawVal, originalPtr, 8);

                    if (rawVal == 0 || rawVal == 0xFFFFFFFFFFFFFFFF ||
                        rawVal < 0x0000010000000000ULL || rawVal > 0x00007FFFFFFFFFFFULL)
                        continue;

                    char strBuf[4096] = {0};
                    if (!SafeReadStringPointerDirect(originalPtr, strBuf, sizeof(strBuf)))
                        continue;

                    size_t len = strlen(strBuf);
                    if (len == 0 || len >= 2048)
                        continue;

                    bool validString = true;
                    for (size_t c = 0; c < len; c++)
                    {
                        unsigned char ch = static_cast<unsigned char>(strBuf[c]);
                        if (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r')
                        {
                            validString = false;
                            break;
                        }
                    }

                    if (!validString)
                        continue;

                    uint32_t stringOffset = (uint32_t)defaultStrings.size();
                    defaultStrings.insert(defaultStrings.end(), strBuf, strBuf + len + 1);

                    memcpy(&sanitizedData[offset], &stringOffset, 4);
                    memset(&sanitizedData[offset + 4], 0, 4);

                    untrackedStringOffsets.push_back(static_cast<uint16_t>(offset));
                }
            }

            for (uint16_t i = 0; i < uiAsset->argDefaultValueSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)sanitizedData[i];
        }
        json << std::dec << "\",\n";

        // Default strings hex
        json << "  \"defaultStringsHex\": \"";
        for (size_t i = 0; i < defaultStrings.size(); i++)
            json << std::hex << std::setfill('0') << std::setw(2) << (int)defaultStrings[i];
        json << std::dec << "\",\n";

        if (!untrackedStringOffsets.empty())
        {
            json << "  \"untrackedStringOffsets\": [";
            for (size_t i = 0; i < untrackedStringOffsets.size(); i++)
            {
                if (i > 0) json << ", ";
                json << untrackedStringOffsets[i];
            }
            json << "],\n";
        }

        // Raw header
        UIAssetHeader_t* hdr = reinterpret_cast<UIAssetHeader_t*>(pakAsset->header());
        uint32_t headerSize = pakAsset->data()->headerStructSize;
        json << "  \"headerSize\": " << headerSize << ",\n";
        json << "  \"headerHex\": \"";
        {
            uint8_t* headerData = reinterpret_cast<uint8_t*>(pakAsset->header());
            for (size_t i = 0; i < headerSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)headerData[i];
        }
        json << std::dec << "\",\n";

        // CPU data page
        size_t cpuDataSize = GetUICpuDataSize(pakAsset);
        json << "  \"hasCpuData\": " << (pakAsset->data()->HasValidDataPagePointer() ? "true" : "false") << ",\n";
        json << "  \"cpuDataSize\": " << cpuDataSize << ",\n";
        json << "  \"cpuDataHex\": \"";
        if (cpuDataSize > 0 && pakAsset->cpu())
        {
            uint8_t* cpuData = reinterpret_cast<uint8_t*>(pakAsset->cpu());
            for (size_t i = 0; i < cpuDataSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)cpuData[i];
        }
        json << std::dec << "\",\n";

        // RUIP conversion fields
        json << "  \"ruiDataStructSize\": " << hdr->ruiDataStructSize << ",\n";
        json << "  \"keyframingCount\": " << hdr->unk_10 << ",\n";
        json << "  \"styleDescriptorCount\": " << hdr->styleDescriptorCount << ",\n";
        json << "  \"renderJobCount\": " << hdr->renderJobCount << ",\n";
        json << "  \"unk_A4\": " << hdr->unk_A4 << ",\n";

        // ArgClusters
        json << "  \"argClustersHex\": \"";
        if (hdr->argClusters && hdr->argClusterCount > 0)
        {
            uint8_t* clustersData = reinterpret_cast<uint8_t*>(hdr->argClusters);
            size_t clustersSize = hdr->argClusterCount * sizeof(UIAssetArgCluster_t);
            for (size_t i = 0; i < clustersSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)clustersData[i];
        }
        json << std::dec << "\",\n";

        // StyleDescriptors
        json << "  \"styleDescriptorsHex\": \"";
        if (hdr->styleDescriptors && hdr->styleDescriptorCount > 0)
        {
            size_t styleDescSize = GetStyleDescriptorSize(pakAsset->version());
            size_t totalStyleSize = hdr->styleDescriptorCount * styleDescSize;
            uint8_t* styleData = reinterpret_cast<uint8_t*>(hdr->styleDescriptors);
            for (size_t i = 0; i < totalStyleSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)styleData[i];
        }
        json << std::dec << "\",\n";

        // RenderJobs - calculate size by parsing widget types
        size_t renderJobsSize = 0;
        bool isV401 = false;
        bool isV421 = false;

        if (hdr->renderJobs && hdr->renderJobCount > 0)
        {
            if (pakAsset->version() == 42)
            {
                const std::string containerName = pakAsset->GetContainerFileName();
                if (containerName.find("ui_v421") != std::string::npos)
                    isV421 = true;
            }
            else if (pakAsset->version() == 40)
            {
                const std::string containerName = pakAsset->GetContainerFileName();
                if (containerName.find("ui_s12") != std::string::npos)
                    isV401 = true;
            }

            size_t offset = 0;
            for (int jobIdx = 0; jobIdx < hdr->renderJobCount; jobIdx++)
            {
                uint16_t widgetType = *reinterpret_cast<uint16_t*>(hdr->renderJobs + offset);
                offset += GetWidgetSize(widgetType, pakAsset->version(), isV401, isV421);
            }
            renderJobsSize = offset;
        }

        json << "  \"renderJobsSize\": " << renderJobsSize << ",\n";
        if (isV421)
            json << "  \"renderJobsVersion\": \"v42.1\",\n";
        else if (pakAsset->version() == 42)
            json << "  \"renderJobsVersion\": \"v42\",\n";
        else if (isV401)
            json << "  \"renderJobsVersion\": \"v40.1\",\n";
        json << "  \"renderJobsHex\": \"";
        if (renderJobsSize > 0 && renderJobsSize < 1024 * 1024)
        {
            for (size_t i = 0; i < renderJobsSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)hdr->renderJobs[i];
        }
        json << std::dec << "\",\n";

        // TransformData - calculate size from adjacent pointers
        size_t transformDataSize = 0;
        if (hdr->transformData)
        {
            std::vector<uintptr_t> transformPointers;
            if (hdr->defaultValues) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->defaultValues));
            if (hdr->transformData) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->transformData));
            if (hdr->argNames) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->argNames));
            if (hdr->argClusters) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->argClusters));
            if (hdr->args) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->args));
            if (hdr->styleDescriptors) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->styleDescriptors));
            if (hdr->renderJobs) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->renderJobs));
            if (hdr->mappingData) transformPointers.push_back(reinterpret_cast<uintptr_t>(hdr->mappingData));

            std::sort(transformPointers.begin(), transformPointers.end());

            uintptr_t transformAddr = reinterpret_cast<uintptr_t>(hdr->transformData);
            for (size_t i = 0; i < transformPointers.size(); i++)
            {
                if (transformPointers[i] == transformAddr && i + 1 < transformPointers.size())
                {
                    transformDataSize = transformPointers[i + 1] - transformAddr;
                    break;
                }
            }
        }

        json << "  \"transformDataSize\": " << transformDataSize << ",\n";
        json << "  \"transformDataHex\": \"";
        if (transformDataSize > 0 && transformDataSize < 1024 * 1024)
        {
            uint8_t* transformBytes = hdr->transformData;
            for (size_t i = 0; i < transformDataSize; i++)
                json << std::hex << std::setfill('0') << std::setw(2) << (int)transformBytes[i];
        }
        json << std::dec << "\",\n";

        // Keyframings - serialize UIAssetMapping_t array + float data
        std::vector<uint8_t> keyframingsBuffer;
        if (hdr->mappingData && hdr->unk_10 > 0)
        {
            int keyframingCount = hdr->unk_10;
            UIAssetMapping_t* mappings = hdr->mappingData;

            size_t mappingHeaderSize = keyframingCount * sizeof(UIAssetMapping_t);
            size_t totalFloatDataSize = 0;
            for (int i = 0; i < keyframingCount; i++)
            {
                uint32_t dataCount = mappings[i].dataCount;
                uint16_t dimension = mappings[i].unk_4;
                uint16_t hermite = mappings[i].unk_6;

                int hermiteMult = (hermite != 0) ? 2 : 1;
                size_t totalFloats = dataCount + (size_t)dimension * dataCount * hermiteMult;
                totalFloatDataSize += totalFloats * sizeof(float);
            }

            keyframingsBuffer.resize(mappingHeaderSize + totalFloatDataSize);

            size_t floatDataOffset = mappingHeaderSize;
            for (int i = 0; i < keyframingCount; i++)
            {
                UIAssetMapping_t entry = mappings[i];

                uint32_t dataCount = entry.dataCount;
                uint16_t dimension = entry.unk_4;
                uint16_t hermite = entry.unk_6;

                int hermiteMult = (hermite != 0) ? 2 : 1;
                size_t totalFloats = dataCount + (size_t)dimension * dataCount * hermiteMult;
                size_t floatDataSize = totalFloats * sizeof(float);

                if (entry.data && floatDataSize > 0)
                    memcpy(&keyframingsBuffer[floatDataOffset], entry.data, floatDataSize);

                entry.data = reinterpret_cast<float*>(floatDataOffset);
                memcpy(&keyframingsBuffer[i * sizeof(UIAssetMapping_t)], &entry, sizeof(UIAssetMapping_t));

                floatDataOffset += floatDataSize;
            }
        }

        json << "  \"keyframingsSize\": " << keyframingsBuffer.size() << ",\n";
        json << "  \"keyframingsHex\": \"";
        for (size_t i = 0; i < keyframingsBuffer.size(); i++)
            json << std::hex << std::setfill('0') << std::setw(2) << (int)keyframingsBuffer[i];
        json << std::dec << "\",\n";

        // ArgNames string table
        json << "  \"argNamesHex\": \"";
        if (hdr->argNames && uiAsset->GetArgNames() && UI_IsMemoryReadable(uiAsset->GetArgNames(), 1))
        {
            size_t argNamesSize = 0;
            for (int i = 0; i < uiAsset->argCount; i++)
            {
                uint16_t nameOffsetVal = uiAsset->GetArgs()[i].nameOffset;
                if (nameOffsetVal > 0)
                {
                    const char* nameStr = uiAsset->GetArgNames() + nameOffsetVal;
                    if (UI_IsMemoryReadable(nameStr, 1))
                    {
                        size_t endPos = nameOffsetVal + strnlen(nameStr, 256) + 1;
                        if (endPos > argNamesSize)
                            argNamesSize = endPos;
                    }
                }
            }
            if (argNamesSize > 0 && UI_IsMemoryReadable(uiAsset->GetArgNames(), argNamesSize))
            {
                uint8_t* namesData = reinterpret_cast<uint8_t*>(const_cast<char*>(uiAsset->GetArgNames()));
                for (size_t i = 0; i < argNamesSize; i++)
                    json << std::hex << std::setfill('0') << std::setw(2) << (int)namesData[i];
            }
        }
        json << std::dec << "\",\n";

        // Pointer fixups
        {
            struct RuiPointerFixup {
                std::string srcSection;
                uint32_t srcOffset;
                const void* dstPtr;
                std::string fieldName;
            };

            std::vector<RuiPointerFixup> pointerFixups;

            // Header pointers
            if (hdr->name)           pointerFixups.push_back({"header", 0x00, hdr->name, "name"});
            if (hdr->defaultValues)  pointerFixups.push_back({"header", 0x08, hdr->defaultValues, "defaultValues"});
            if (hdr->transformData)  pointerFixups.push_back({"header", 0x10, hdr->transformData, "transformData"});
            if (hdr->argNames)       pointerFixups.push_back({"header", 0x28, hdr->argNames, "argNames"});
            if (hdr->argClusters)    pointerFixups.push_back({"header", 0x30, hdr->argClusters, "argClusters"});
            if (hdr->args)           pointerFixups.push_back({"header", 0x38, hdr->args, "args"});
            if (hdr->styleDescriptors) pointerFixups.push_back({"header", 0x50, hdr->styleDescriptors, "styleDescriptors"});
            if (hdr->renderJobs)     pointerFixups.push_back({"header", 0x58, hdr->renderJobs, "renderJobs"});
            if (hdr->mappingData)    pointerFixups.push_back({"header", 0x60, hdr->mappingData, "mappingData"});

            // String argument pointers in defaultValues
            struct StringArgFixup {
                uint32_t srcOffset;
                const char* stringPtr;
                std::string fieldName;
                std::string stringValue;
                bool isExternal;
            };
            std::vector<StringArgFixup> stringArgFixups;

            auto isValidPointer = [](const void* ptr) -> bool {
                uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
                if (addr == 0 || addr < 0x10000 || addr > 0x00007FFFFFFFFFFF) return false;
                if (addr == 0xCDCDCDCDCDCDCDCD || addr == 0xDDDDDDDDDDDDDDDD || addr == 0xFEEEFEEEFEEEFEEE) return false;
                return true;
            };

            if (hdr->args && hdr->defaultValues &&
                UI_IsMemoryReadable(hdr->args, hdr->argCount * sizeof(UIAssetArg_t)) &&
                UI_IsMemoryReadable(hdr->defaultValues, hdr->defaultValuesSize))
            {
                for (int16_t argIdx = 0; argIdx < hdr->argCount; argIdx++)
                {
                    const UIAssetArg_t& arg = hdr->args[argIdx];

                    bool isPointerType = (arg.type == UI_ARG_TYPE_STRING || arg.type == UI_ARG_TYPE_ASSET ||
                                          arg.type == UI_ARG_TYPE_UIHANDLE || arg.type == UI_ARG_TYPE_IMAGE);

                    if (isPointerType && arg.dataOffset + 8 <= hdr->defaultValuesSize)
                    {
                        const char* ptrLocation = reinterpret_cast<const char*>(hdr->defaultValues) + arg.dataOffset;
                        if (!UI_IsMemoryReadable(ptrLocation, 8))
                            continue;
                        const char* stringPtr = *reinterpret_cast<const char* const*>(ptrLocation);

                        if (isValidPointer(stringPtr) && UI_IsMemoryReadable(stringPtr, 1))
                        {
                            std::string strValue;
                            size_t maxReadable = 256;
                            if (UI_IsMemoryReadable(stringPtr, maxReadable))
                            {
                                size_t len = strnlen(stringPtr, maxReadable);
                                if (len > 0 && len < maxReadable)
                                    strValue = std::string(stringPtr, len);
                            }

                            stringArgFixups.push_back({arg.dataOffset, stringPtr,
                                "arg_" + std::to_string(argIdx) + "_string", strValue, false});
                        }
                    }
                }
            }

            // Determine CPU data range from header pointers
            const char* cpuDataBase = nullptr;
            const char* cpuDataEnd = nullptr;
            for (const auto& pf : pointerFixups)
            {
                const char* ptr = reinterpret_cast<const char*>(pf.dstPtr);
                if (ptr)
                {
                    if (!cpuDataBase || ptr < cpuDataBase) cpuDataBase = ptr;
                    if (!cpuDataEnd || ptr > cpuDataEnd) cpuDataEnd = ptr;
                }
            }

            size_t cpuRangeSize = cpuDataBase && cpuDataEnd ? static_cast<size_t>(cpuDataEnd - cpuDataBase) + 8192 : 0;
            if (cpuRangeSize > 1024 * 1024) cpuRangeSize = 1024 * 1024;

            // Classify string fixups as internal or external
            size_t externalStrings = 0;
            for (auto& saf : stringArgFixups)
            {
                if (cpuDataBase && saf.stringPtr >= cpuDataBase && saf.stringPtr < cpuDataBase + cpuRangeSize)
                {
                    saf.isExternal = false;
                    pointerFixups.push_back({"defaultValues", saf.srcOffset, saf.stringPtr, saf.fieldName});
                }
                else
                {
                    saf.isExternal = true;
                    externalStrings++;
                }
            }

            // Export pointer fixups
            json << "  \"pointerFixups\": [\n";
            for (size_t i = 0; i < pointerFixups.size(); i++)
            {
                const auto& pf = pointerFixups[i];
                const char* dstPtr = reinterpret_cast<const char*>(pf.dstPtr);
                int64_t dstCpuOffset = cpuDataBase ? (dstPtr - cpuDataBase) : 0;

                json << "    { \"srcSection\": \"" << pf.srcSection << "\""
                     << ", \"srcOffset\": " << pf.srcOffset
                     << ", \"dstCpuOffset\": " << dstCpuOffset
                     << ", \"field\": \"" << pf.fieldName << "\" }";
                if (i < pointerFixups.size() - 1 || externalStrings > 0)
                    json << ",";
                json << "\n";
            }

            // External string fixups
            size_t extIdx = 0;
            for (const auto& saf : stringArgFixups)
            {
                if (!saf.isExternal)
                    continue;

                std::string escapedStr;
                for (char c : saf.stringValue)
                {
                    if (c == '"') escapedStr += "\\\"";
                    else if (c == '\\') escapedStr += "\\\\";
                    else if (c == '\n') escapedStr += "\\n";
                    else if (c == '\r') escapedStr += "\\r";
                    else if (c == '\t') escapedStr += "\\t";
                    else if (c >= 32 && c < 127) escapedStr += c;
                    else {
                        char hex[8];
                        snprintf(hex, sizeof(hex), "\\u%04x", (unsigned char)c);
                        escapedStr += hex;
                    }
                }

                json << "    { \"srcSection\": \"defaultValues\""
                     << ", \"srcOffset\": " << saf.srcOffset
                     << ", \"external\": true"
                     << ", \"stringValue\": \"" << escapedStr << "\""
                     << ", \"field\": \"" << saf.fieldName << "\" }";
                extIdx++;
                if (extIdx < externalStrings)
                    json << ",";
                json << "\n";
            }

            json << "  ]\n";
        }

        json << "}\n";

        exportPath.replace_extension(".json");

        StreamIO out;
        if (!out.open(exportPath.string(), eStreamIOMode::Write))
        {
            assertm(false, "Failed to open file for write.");
            return false;
        }

        out.write(json.str().c_str(), json.str().length());
        out.close();

        return true;
    }
    default:
        break;
    }

    unreachable();
}

void InitUIAssetType()
{
    static const char* settings[] = { "JSON" };
    AssetTypeBinding_t type =
    {
        .name = "RUI",
        .type = '\0iu',
        .headerAlignment = 8,
        .loadFunc = LoadUIAsset,
        .postLoadFunc = PostLoadUIAsset,
        .previewFunc = PreviewUIAsset,
        .e = { ExportUIAsset, 0, settings, ARRAYSIZE(settings) },
    };

    REGISTER_TYPE(type);
}
