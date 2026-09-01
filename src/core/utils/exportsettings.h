#pragma once

#include <core/utils/cli_parser.h>
#include <filesystem>

struct ExportSettings_t
{
    // texture
    uint32_t exportNormalRecalcSetting;
    uint32_t exportTextureNameSetting;

    bool exportMaterialTextures;

    // misc
    bool exportPathsFull;
    bool exportAssetDeps;
    bool exportAssetDependents;
    bool disableCachedNames;

    // model settings
    uint32_t previewedSkinIndex;
    
    // what qc version to target
    uint16_t qcMajorVersion;
    uint16_t qcMinorVersion;

    bool exportRigSequences;        // export sequences with a model or rig
    bool exportModelSkin;           // export the selected skin for a model
    bool exportModelMatsTruncated;  // truncate material names in model files
    bool exportQCIFiles;            // qc will split into multiple include files

    // keep SM5.1 shader bytecode instead of substituting an SM5.0 sibling;
    // the substitution is only needed when the target device is DX11
    bool shaderKeepSM51;

    // model physics settings
    uint32_t exportPhysicsContentsFilter;
    bool exportPhysicsFilterExclusive;
    bool exportPhysicsFilterAND;

    std::filesystem::path exportDirectory;

    void SetFromCLI(const CCommandLine* cli);

    void SetExportDirectory(const std::filesystem::path& exportPath)
    {
        this->exportDirectory = exportPath;
    }

    const std::filesystem::path& GetExportDirectory() const
    {
        return this->exportDirectory;
    }
};

enum eNormalExportRecalc : uint32_t
{
    NML_RECALC_NONE,
    NML_RECALC_DX,
    NML_RECALC_OGL,

    NML_RECALC_COUNT,
};

static const char* s_NormalExportRecalcSetting[eNormalExportRecalc::NML_RECALC_COUNT] =
{
    "None",
    "DirectX",
    "OpenGL",
};

enum eTextureExportName : uint32_t
{
    TXTR_NAME_GUID,
    TXTR_NAME_REAL, // only use real values for name export (will fall back on guid if texture asset's name is nullptr)
    TXTR_NAME_TEXT, // use a text name for textures (will fall back on a generated semantic if texture asset's name is nullptr)
    TXTR_NAME_SMTC, // always use a generated name for textures (for exporting to things such as cast)

    TXTR_NAME_COUNT,
};

static const char* s_TextureExportNameSetting[eTextureExportName::TXTR_NAME_COUNT] =
{
    "GUID",
    "Real",
    "Text",
    "Semantic",
};

enum eCompressionLevel : uint32_t
{
    CMPR_LVL_NONE = 0,    // OodleLZ_CompressionLevel_None
    CMPR_LVL_SUPERFAST,   // OodleLZ_CompressionLevel_SuperFast
    CMPR_LVL_VERYFAST,    // OodleLZ_CompressionLevel_VeryFast
    CMPR_LVL_FAST,        // OodleLZ_CompressionLevel_Fast
    CMPR_LVL_NORMAL,      // OodleLZ_CompressionLevel_Normal

    CMPR_LVL_COUNT,
};

static const char* s_CompressionLevelSetting[eCompressionLevel::CMPR_LVL_COUNT] =
{
    "None",
    "Super Fast",
    "Very Fast",
    "Fast",
    "Normal"
};

// preview settings
#define PREVIEW_CULL_DEFAULT    1000.0f
#define PREVIEW_CULL_MIN        256.0f // map max size
#define PREVIEW_CULL_MAX        16384.0f // quarter of max map size

#define PREVIEW_SPEED_DEFAULT   10.0f
#define PREVIEW_SPEED_MIN       1.0f
#define PREVIEW_SPEED_MAX       200.0f
#define PREVIEW_SPEED_MULT      5.0f

struct PreviewSettings_t
{
    float previewCullDistance; // currently can only be set on load, which is not ideal
    float previewMovementSpeed;
};