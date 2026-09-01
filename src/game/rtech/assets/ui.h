#pragma once

struct UIAssetArgCluster_t
{
    uint16_t argIndex;
    uint16_t argCount;

    uint8_t byte_4;
    uint8_t byte_5;

    char gap_6[4];

    uint16_t word_A;

    char gap_C[6];
};

enum UIAssetArgType_t : uint8_t
{
    UI_ARG_TYPE_NONE = 0,
    UI_ARG_TYPE_STRING = 0x1,    // char** -> const char*
    UI_ARG_TYPE_ASSET = 0x2,     // char** -> const char* (asset path)
    UI_ARG_TYPE_BOOL = 0x3,
    UI_ARG_TYPE_INT = 0x4,
    UI_ARG_TYPE_FLOAT = 0x5,
    UI_ARG_TYPE_FLOAT2 = 0x6,
    UI_ARG_TYPE_FLOAT3 = 0x7,
    UI_ARG_TYPE_COLOR_ALPHA = 0x8, // float[4] RGBA
    UI_ARG_TYPE_GAMETIME = 0x9,
    UI_ARG_TYPE_WALLTIME = 0xA,  // int64_t
    UI_ARG_TYPE_UIHANDLE = 0xB,
    UI_ARG_TYPE_IMAGE = 0xC,
    UI_ARG_TYPE_FONT_FACE = 0xD,
    UI_ARG_TYPE_FONT_HASH = 0xE,
    UI_ARG_TYPE_ARRAY = 0xF,
};

static const char* const s_UIArgTypeNames[] = {
    "NONE",
    "STRING",
    "ASSET",
    "BOOL",
    "INT",
    "FLOAT",
    "FLOAT2",
    "FLOAT3",
    "COLOR ALPHA",
    "GAMETIME",
    "WALLTIME",
    "UIHANDLE",
    "IMAGE",
    "FONT FACE",
    "FONT HASH",
    "ARRAY",
};

inline size_t GetUIArgValueSize(UIAssetArgType_t type)
{
    switch (type)
    {
    case UI_ARG_TYPE_NONE:        return 0;
    case UI_ARG_TYPE_BOOL:        return 1;
    case UI_ARG_TYPE_INT:         return 4;
    case UI_ARG_TYPE_FLOAT:       return 4;
    case UI_ARG_TYPE_GAMETIME:    return 4;
    case UI_ARG_TYPE_FLOAT2:      return 8;
    case UI_ARG_TYPE_FLOAT3:      return 12;
    case UI_ARG_TYPE_COLOR_ALPHA: return 16;
    case UI_ARG_TYPE_WALLTIME:    return 8;
    case UI_ARG_TYPE_STRING:
    case UI_ARG_TYPE_ASSET:
    case UI_ARG_TYPE_IMAGE:
    case UI_ARG_TYPE_UIHANDLE:    return 8;
    default:                      return 0;
    }
}

union UIAssetArgValue_t
{
    char* rawptr;
    char** string;
    bool* boolean;
    int* integer;
    float* fpn;
    int64_t* integer64;
};

struct UIAssetArg_t
{
    UIAssetArgType_t type;  // +0x00
    uint8_t unk_1;          // +0x01
    uint16_t dataOffset;    // +0x02: offset into defaultValues
    uint16_t nameOffset;    // +0x04: offset into argNames (V29/V30 only)
    uint16_t shortHash;     // +0x06: arg hash (primary identifier in V39+)
};
static_assert(sizeof(UIAssetArg_t) == 8, "UIAssetArg_t must be 8 bytes");

// V30 style descriptor - 52 bytes
struct UIAssetStyleDescriptor_v30_t
{
    uint16_t type;              // +0x00: UiWidgetType_e
    uint16_t color[3][4];       // +0x02
    uint16_t tint[4];           // +0x1A
    uint16_t blend;             // +0x22
    uint16_t premul;            // +0x24
    uint16_t hue;               // +0x26
    uint16_t saturation;        // +0x28
    uint16_t lightness;         // +0x2A

    uint16_t fontHash;          // +0x2C
    uint16_t shadowAlpha;       // +0x2E
    uint16_t shadowOffset[2];   // +0x30
};
static_assert(sizeof(UIAssetStyleDescriptor_v30_t) == 52, "UIAssetStyleDescriptor_v30_t must be 52 bytes");

// V39+ style descriptor - 68 bytes
struct UIAssetStyleDescriptor_v39_t
{
    uint16_t type;              // +0x00: UiWidgetType_e
    uint16_t color[3][4];       // +0x02
    uint16_t tint[4];           // +0x1A
    uint16_t blend;             // +0x22
    uint16_t premul;            // +0x24
    uint16_t hue;               // +0x26
    uint16_t saturation;        // +0x28
    uint16_t lightness;         // +0x2A

    uint16_t fontHash;          // +0x2C
    uint16_t shadowAlpha;       // +0x2E
    uint16_t shadowOffset[2];   // +0x30
    uint16_t shadowBlur;        // +0x34
    uint16_t pixelHeight;       // +0x36
    uint16_t pixelAspect;       // +0x38
    uint16_t outlineWidth;      // +0x3A
    uint16_t thicken;           // +0x3C
    uint16_t blur;              // +0x3E
    uint16_t baselineShift;     // +0x40
    uint16_t kerning;           // +0x42
};
static_assert(sizeof(UIAssetStyleDescriptor_v39_t) == 68, "UIAssetStyleDescriptor_v39_t must be 68 bytes");

using UIAssetStyleDescriptor_t = UIAssetStyleDescriptor_v39_t;

inline size_t GetStyleDescriptorSize(uint32_t version)
{
    return (version >= 39) ? sizeof(UIAssetStyleDescriptor_v39_t) : sizeof(UIAssetStyleDescriptor_v30_t);
}

enum UiWidgetType_e : uint16_t
{
    UI_WIDGET_TEXT = 0,
    UI_WIDGET_ASSET = 1,
    UI_WIDGET_ELLIPSE = 2,
    UI_WIDGET_CAMERA = 3,
    UI_WIDGET_VIDEO = 4,
    UI_WIDGET_NESTED = 5,
};

// V39 widget sizes
static constexpr uint32_t V39_WIDGET_SIZES[] = {
    22,   // Text
    46,   // Asset
    28,   // Ellipse
    28,   // Camera
    46,   // Video
    12,   // Nested
};

// V40 widget sizes - Text +2 vs V39
static constexpr uint32_t V40_WIDGET_SIZES[] = {
    24,   // Text
    46,   // Asset
    28,   // Ellipse
    28,   // Camera
    46,   // Video
    12,   // Nested
};

// V40.1 widget sizes - all +2 vs V40 (new uint16 field in header)
static constexpr uint32_t V401_WIDGET_SIZES[] = {
    26,   // Text
    48,   // Asset
    30,   // Ellipse
    30,   // Camera
    48,   // Video
    14,   // Nested
};

// V42 widget sizes - Asset +2 vs V40.1
static constexpr uint32_t V42_WIDGET_SIZES[] = {
    26,   // Text
    50,   // Asset
    30,   // Ellipse
    30,   // Camera
    48,   // Video
    14,   // Nested
};

// V42.1 widget sizes - Text +2 vs V42
static constexpr uint32_t V421_WIDGET_SIZES[] = {
    28,   // Text
    50,   // Asset
    30,   // Ellipse
    30,   // Camera
    48,   // Video
    14,   // Nested
};

inline uint32_t GetWidgetSize(uint16_t type, uint32_t version, bool isV401 = false, bool isV421 = false)
{
    if (type > 5) return 46; // default to Asset size
    if (isV421) return V421_WIDGET_SIZES[type];
    if (version == 42) return V42_WIDGET_SIZES[type];
    if (isV401) return V401_WIDGET_SIZES[type];
    return (version >= 40) ? V40_WIDGET_SIZES[type] : V39_WIDGET_SIZES[type];
}

inline bool DetectV401RenderJobs(const uint8_t* renderJobs, uint16_t count, size_t actualDataSize = 0)
{
    if (!renderJobs || count == 0)
        return false;

    size_t v40Total = 0;
    bool v40Valid = true;
    for (uint16_t i = 0; i < count && v40Valid; i++)
    {
        uint16_t type = *reinterpret_cast<const uint16_t*>(renderJobs + v40Total);
        if (type > 5) { v40Valid = false; break; }
        v40Total += V40_WIDGET_SIZES[type];
    }

    size_t v401Total = 0;
    bool v401Valid = true;
    for (uint16_t i = 0; i < count && v401Valid; i++)
    {
        uint16_t type = *reinterpret_cast<const uint16_t*>(renderJobs + v401Total);
        if (type > 5) { v401Valid = false; break; }
        v401Total += V401_WIDGET_SIZES[type];
    }

    if (actualDataSize > 0)
    {
        if (v401Valid && v401Total == actualDataSize) return true;
        if (v40Valid && v40Total == actualDataSize) return false;
    }

    if (v401Valid && !v40Valid) return true;
    return false;
}

inline size_t CalculateRenderJobsSize(const uint8_t* renderJobs, uint16_t count, uint32_t version, bool isV401 = false, bool isV421 = false)
{
    if (!renderJobs || count == 0) return 0;

    size_t totalSize = 0;
    size_t offset = 0;

    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t type = *reinterpret_cast<const uint16_t*>(renderJobs + offset);
        uint32_t widgetSize = GetWidgetSize(type, version, isV401, isV421);
        totalSize += widgetSize;
        offset += widgetSize;
    }

    return totalSize;
}

// Common widget header - 8 bytes
#pragma pack(push, 1)
struct UiWidgetHdr_s
{
    uint16_t type;          // +0x00: UiWidgetType_e
    uint16_t drawFlags;     // +0x02
    uint16_t xfrmIdx;       // +0x04
    uint16_t clipXfrmIdx;   // +0x06
};
static_assert(sizeof(UiWidgetHdr_s) == 8, "UiWidgetHdr_s must be 8 bytes");

// V39 Text widget - 22 bytes
struct UiWidget_Text_v39_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t fontStyleIdx;          // +0x08
    uint16_t text;                  // +0x0A
    uint16_t fitToWidth;            // +0x0C
    uint16_t fitToHeight;           // +0x0E
    uint16_t lineBreakWidth;        // +0x10
    uint16_t textAlign;             // +0x12
    uint16_t styleIdx;              // +0x14
};
static_assert(sizeof(UiWidget_Text_v39_s) == 22, "UiWidget_Text_v39_s must be 22 bytes");

// V39 Asset widget - 46 bytes
struct UiWidget_Asset_v39_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t assetIndex_0;          // +0x08
    uint16_t assetIndex_1;          // +0x0A
    uint16_t mins[2];               // +0x0C
    uint16_t maxs[2];               // +0x10
    uint16_t uvMins[2];             // +0x14
    uint16_t uvMaxs[2];             // +0x18
    uint16_t maskCenter[2];         // +0x1C
    uint16_t maskRotation;          // +0x20
    uint16_t maskTranslate[2];      // +0x22
    uint16_t maskSize[2];           // +0x26
    uint16_t flags;                 // +0x2A
    uint8_t styleIdx;               // +0x2C
    uint8_t _padding;               // +0x2D
};
static_assert(sizeof(UiWidget_Asset_v39_s) == 46, "UiWidget_Asset_v39_s must be 46 bytes");

// V39 Ellipse widget - 28 bytes
struct UiWidget_Ellipse_v39_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t innerRadius;           // +0x08
    uint16_t outerRadius;           // +0x0A
    uint16_t startAngle;            // +0x0C
    uint16_t endAngle;              // +0x0E
    uint16_t segments;              // +0x10
    uint16_t fillMode;              // +0x12
    uint16_t unk_14;                // +0x14
    uint16_t unk_16;                // +0x16
    uint16_t opts;                  // +0x18
    uint8_t styleIdx;               // +0x1A
    uint8_t _padding;               // +0x1B
};
static_assert(sizeof(UiWidget_Ellipse_v39_s) == 28, "UiWidget_Ellipse_v39_s must be 28 bytes");

// V39 Camera widget - same layout as Ellipse
using UiWidget_Camera_v39_s = UiWidget_Ellipse_v39_s;

// V39 Video widget - 46 bytes
struct UiWidget_Video_v39_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t videoChannel;          // +0x08
    uint16_t clipMin[2];            // +0x0A
    uint16_t clipMax[2];            // +0x0E
    uint16_t uv0_min[2];           // +0x12
    uint16_t uv0_max[2];           // +0x16
    uint16_t unk_1A[9];            // +0x1A
    uint8_t styleIdx;               // +0x2C
    uint8_t _padding;               // +0x2D
};
static_assert(sizeof(UiWidget_Video_v39_s) == 46, "UiWidget_Video_v39_s must be 46 bytes");

// V39 Nested widget - 12 bytes
struct UiWidget_Nested_v39_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t uiHandle;             // +0x08
    uint8_t styleIdx;               // +0x0A
    uint8_t _padding;               // +0x0B
};
static_assert(sizeof(UiWidget_Nested_v39_s) == 12, "UiWidget_Nested_v39_s must be 12 bytes");
#pragma pack(pop)

// V40 Text widget - 24 bytes (V39 + new unk_10 field)
#pragma pack(push, 1)
struct UiWidget_Text_v40_s
{
    UiWidgetHdr_s hdr;              // +0x00
    uint16_t fontStyleIdx;          // +0x08
    uint16_t text;                  // +0x0A
    uint16_t fitToWidth;            // +0x0C
    uint16_t fitToHeight;           // +0x0E
    uint16_t unk_10;                // +0x10: new in V40
    uint16_t lineBreakWidth;        // +0x12
    uint16_t textAlign;             // +0x14
    uint16_t styleIdx;              // +0x16
};
static_assert(sizeof(UiWidget_Text_v40_s) == 24, "UiWidget_Text_v40_s must be 24 bytes");

// V40 other widgets are identical to V39
using UiWidget_Video_v40_s = UiWidget_Video_v39_s;
using UiWidget_Nested_v40_s = UiWidget_Nested_v39_s;
using UiWidget_Asset_v40_s = UiWidget_Asset_v39_s;
using UiWidget_Ellipse_v40_s = UiWidget_Ellipse_v39_s;
using UiWidget_Camera_v40_s = UiWidget_Camera_v39_s;
#pragma pack(pop)

// V40.1 widget header - 10 bytes (V40 header + new unk_02 field)
#pragma pack(push, 1)
struct UiWidgetHdr_v401_s
{
    uint16_t type;          // +0x00
    uint16_t unk_02;        // +0x02: new in V40.1
    uint16_t drawFlags;     // +0x04
    uint16_t xfrmIdx;       // +0x06
    uint16_t clipXfrmIdx;   // +0x08
};
static_assert(sizeof(UiWidgetHdr_v401_s) == 10, "UiWidgetHdr_v401_s must be 10 bytes");

// V40.1 Text widget - 26 bytes
struct UiWidget_Text_v401_s
{
    UiWidgetHdr_v401_s hdr;         // +0x00
    uint16_t fontStyleIdx;          // +0x0A
    uint16_t text;                  // +0x0C
    uint16_t fitToWidth;            // +0x0E
    uint16_t fitToHeight;           // +0x10
    uint16_t unk_12;                // +0x12
    uint16_t lineBreakWidth;        // +0x14
    uint16_t textAlign;             // +0x16
    uint16_t styleIdx;              // +0x18
};
static_assert(sizeof(UiWidget_Text_v401_s) == 26, "UiWidget_Text_v401_s must be 26 bytes");

// V40.1 Nested widget - 14 bytes
struct UiWidget_Nested_v401_s
{
    UiWidgetHdr_v401_s hdr;         // +0x00
    uint16_t uiHandle;             // +0x0A
    uint8_t styleIdx;               // +0x0C
    uint8_t _padding;               // +0x0D
};
static_assert(sizeof(UiWidget_Nested_v401_s) == 14, "UiWidget_Nested_v401_s must be 14 bytes");
#pragma pack(pop)

struct UIAssetMapping_t
{
    uint32_t dataCount;

    uint16_t unk_4; // dimension
    uint16_t unk_6; // hermite flag

    float* data;
};

// UIAssetHeader_t - 104 bytes
struct UIAssetHeader_t
{
    const char* name;                       // +0x00
    void* defaultValues;                    // +0x08
    uint8_t* transformData;                 // +0x10

    float elementWidth;                     // +0x18
    float elementHeight;                    // +0x1C
    float elementWidthRatio;                // +0x20
    float elementHeightRatio;               // +0x24

    const char* argNames;                   // +0x28: NULL in V39+
    UIAssetArgCluster_t* argClusters;       // +0x30
    UIAssetArg_t* args;                     // +0x38

    int16_t argCount;                       // +0x40
    int16_t unk_10;                         // +0x42: keyframing count

    uint16_t ruiDataStructSize;             // +0x44
    uint16_t defaultValuesSize;             // +0x46
    uint16_t styleDescriptorCount;          // +0x48

    uint16_t unk_A4;                        // +0x4A

    uint16_t renderJobCount;                // +0x4C
    uint16_t argClusterCount;               // +0x4E

    UIAssetStyleDescriptor_t* styleDescriptors; // +0x50
    uint8_t* renderJobs;                    // +0x58
    UIAssetMapping_t* mappingData;          // +0x60
};
static_assert(sizeof(UIAssetHeader_t) == 104, "UIAssetHeader_t must be 104 bytes");

// UIAsset owns copies of all data buffers from the pak header.
// Pak page memory may be freed after loading, so we must not store raw pak pointers.
class UIAsset
{
public:
    UIAsset(UIAssetHeader_t* hdr, int version)
        : elementWidth(hdr->elementWidth)
        , elementHeight(hdr->elementHeight)
        , elementWidthRatio(hdr->elementWidthRatio)
        , elementHeightRatio(hdr->elementHeightRatio)
        , argCount(hdr->argCount)
        , argClusterCount(hdr->argClusterCount)
        , argDefaultValueSize(hdr->defaultValuesSize)
    {
        // Copy name string (always valid - resolved from header page)
        if (hdr->name)
            ownedName = hdr->name;

        // Copy args array (always valid - in header page)
        if (hdr->args && hdr->argCount > 0)
        {
            ownedArgs.resize(hdr->argCount);
            memcpy(ownedArgs.data(), hdr->args, hdr->argCount * sizeof(UIAssetArg_t));
        }

        // Copy arg clusters (always valid - in header page)
        if (hdr->argClusters && hdr->argClusterCount > 0)
        {
            ownedArgClusters.resize(hdr->argClusterCount);
            memcpy(ownedArgClusters.data(), hdr->argClusters, hdr->argClusterCount * sizeof(UIAssetArgCluster_t));
        }

        // Copy defaultValues buffer (CPU data page - may be freed after load)
        if (hdr->defaultValues && hdr->defaultValuesSize > 0)
        {
            ownedDefaultValues.resize(hdr->defaultValuesSize);
            memcpy(ownedDefaultValues.data(), hdr->defaultValues, hdr->defaultValuesSize);
        }

        // Copy argNames string table (V29/V30 only - V39+ has no arg names)
        if (version <= 30 && hdr->argNames && hdr->argCount > 0)
        {
            size_t argNamesSize = 0;
            for (int16_t i = 0; i < hdr->argCount; i++)
            {
                uint16_t nameOff = hdr->args[i].nameOffset;
                if (nameOff > 0)
                {
                    size_t endPos = nameOff + strnlen(hdr->argNames + nameOff, 256) + 1;
                    if (endPos > argNamesSize)
                        argNamesSize = endPos;
                }
            }
            if (argNamesSize > 0)
            {
                ownedArgNames.resize(argNamesSize);
                memcpy(ownedArgNames.data(), hdr->argNames, argNamesSize);
            }
        }
    }

    // Accessors that return pointers into owned buffers (always valid)
    const char* GetName() const { return ownedName.empty() ? nullptr : ownedName.c_str(); }
    UIAssetArg_t* GetArgs() { return ownedArgs.empty() ? nullptr : ownedArgs.data(); }
    const UIAssetArg_t* GetArgs() const { return ownedArgs.empty() ? nullptr : ownedArgs.data(); }
    void* GetDefaultValues() { return ownedDefaultValues.empty() ? nullptr : ownedDefaultValues.data(); }
    const char* GetArgNames() const { return ownedArgNames.empty() ? nullptr : ownedArgNames.data(); }

    bool HasArgNames() const { return !ownedArgNames.empty(); }

    const char* GetCachedString(uint16_t dataOffset) const
    {
        auto it = cachedStrings.find(dataOffset);
        return (it != cachedStrings.end()) ? it->second.c_str() : nullptr;
    }

    void CacheString(uint16_t dataOffset, const char* str)
    {
        if (str && str[0] != '\0')
            cachedStrings[dataOffset] = str;
        else
            cachedStrings[dataOffset] = "";
    }

    // Public fields (scalar values copied by value - always safe)
    std::string ownedName;

    float elementWidth;
    float elementHeight;
    float elementWidthRatio;
    float elementHeightRatio;

    int16_t argCount;
    uint16_t argClusterCount;
    uint16_t argDefaultValueSize;

    // Owned data buffers (copies of pak page data)
    std::vector<UIAssetArg_t> ownedArgs;
    std::vector<UIAssetArgCluster_t> ownedArgClusters;
    std::vector<uint8_t> ownedDefaultValues;
    std::vector<char> ownedArgNames;

    std::unordered_map<uint16_t, std::string> cachedStrings;
};
