#include "pch.h"
#include <game/rtech/assets/weapon_definition.h>

extern ExportSettings_t g_ExportSettings;

void LoadWeaponDefinitionAsset(CAssetContainer* const pak, CAsset* const asset)
{
    UNUSED(pak);
    UNUSED(asset);

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    const WepnAssetHeader_v1_t* const header = reinterpret_cast<const WepnAssetHeader_v1_t* const>(pakAsset->header());

    const std::string assetName = "weapon/" + std::string(header->weaponName) + ".rpak";
    pakAsset->SetAssetName(assetName);
}

std::string R_GetWeaponDefinitionAsString(const WepnData_v1_t* const key, const std::string& indentStr)
{
    std::string retVal = indentStr + "\"" + key->name + "\"\n" + indentStr + "{\n";

    // first loop over child values
    for (int j = 0; j < key->numValues; ++j)
    {
        retVal += indentStr + "\t// val unk: " + std::to_string(key->unk_20[j]) + "\n";
        WepnKeyValue_v1_t* val = &key->childKVPairs[j];
        retVal += indentStr + "\t\"" + std::string(val->key) + "\" \"";

        if (val->valueType == WepnValueType_t::STRING)
            retVal += std::string(val->value.strVal);
        else if (val->valueType == WepnValueType_t::INTEGER)
            retVal += std::to_string(val->value.intVal);
        else if (val->valueType == WepnValueType_t::FLT)
            retVal += std::to_string(val->value.fltVal);
        else
        {
            retVal += "\"unk\" // " + std::string(val->key) + ": unknown. rawval: " + std::format("{:X}", val->value.rawVal);
            Log("WEPN: unknown var type: %s (type %i): %llX\n", val->key, val->valueType, val->value.rawVal);
        }
        retVal += "\"\n";
    }

    // add a newline to separate the values and the objects
    retVal += "\n";

    // then loop over child objects
    for (int i = 0; i < key->numChildren; ++i)
    {
        WepnData_v1_t* childKey = &key->childObjects[i];

        retVal += indentStr + "\t// child unk: " + std::to_string(key->unk_28[i]) + "\n";

        retVal += R_GetWeaponDefinitionAsString(childKey, indentStr + "\t");

        if (i != key->numChildren - 1)
            retVal += "\n"; // as long as we aren't the last child object, add a new line as a separator
    }

    retVal += indentStr + "}\n";

    return retVal;
}

static void R_AppendJsonString(std::string& out, const char* const str)
{
    out += '"';
    for (const char* c = str; *c; ++c)
    {
        const unsigned char ch = static_cast<unsigned char>(*c);
        if (ch == '"' || ch == '\\')
        {
            out += '\\';
            out += *c;
        }
        else if (ch < 0x20)
            out += std::format("\\u{:04x}", ch);
        else
            out += *c;
    }
    out += '"';
}

static void R_AppendOrder(std::string& out, const char* const field, const uint16_t* const order, const int count)
{
    out += std::format(",\"{}\":[", field);
    for (int i = 0; i < count; ++i)
        out += std::format("{}{}", i ? "," : "", order[i]);
    out += ']';
}

// Values carry their exact bits and blocks carry their hash-sorted lookup tables: the
// tie order among duplicate keys decides which one a lookup finds and follows no single
// sort, so a lossless round trip has to keep it.
static void R_AppendWepnBlockJson(std::string& out, const WepnData_v1_t* const block)
{
    out += "{\"name\":";
    R_AppendJsonString(out, block->name);

    out += ",\"values\":[";
    for (int i = 0; i < block->numValues; ++i)
    {
        const WepnKeyValue_v1_t* const kv = &block->childKVPairs[i];

        out += i ? ",{\"key\":" : "{\"key\":";
        R_AppendJsonString(out, kv->key);
        out += std::format(",\"type\":{}", static_cast<uint32_t>(kv->valueType));

        if (kv->valueType == WepnValueType_t::STRING)
        {
            out += ",\"value\":";
            R_AppendJsonString(out, kv->value.strVal);
        }
        else
            out += std::format(",\"raw\":\"0x{:016X}\"", kv->value.rawVal);

        out += '}';
    }
    out += ']';

    if (block->numValues)
        R_AppendOrder(out, "valueOrder", block->unk_20, block->numValues);

    out += ",\"children\":[";
    for (int i = 0; i < block->numChildren; ++i)
    {
        if (i)
            out += ',';
        R_AppendWepnBlockJson(out, &block->childObjects[i]);
    }
    out += ']';

    if (block->numChildren)
        R_AppendOrder(out, "childOrder", block->unk_28, block->numChildren);

    out += '}';
}

// The input format of RePak's wepn/impa writer.
std::string R_GetWepnTreeAsJson(CPakAsset* const pakAsset)
{
    const WepnAssetHeader_v1_t* const header = reinterpret_cast<const WepnAssetHeader_v1_t*>(pakAsset->header());

    std::string out = "{\"name\":";
    R_AppendJsonString(out, header->weaponName);

    std::vector<AssetGuid_t> dependencies;
    pakAsset->getDependencies(dependencies);

    out += ",\"dependencies\":[";
    for (size_t i = 0; i < dependencies.size(); ++i)
        out += std::format("{}\"0x{:016X}\"", i ? "," : "", dependencies[i].guid);
    out += "],\"root\":";

    R_AppendWepnBlockJson(out, header->rootKey);

    out += "}\n";
    return out;
}

bool ExportWeaponDefinitionAsset(CAsset* const asset, const int setting)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    const WepnAssetHeader_v1_t* const header = reinterpret_cast<WepnAssetHeader_v1_t* const>(pakAsset->header());

    const bool asJson = setting == 0;
    const std::string wepnTxt = asJson ? R_GetWepnTreeAsJson(pakAsset) : R_GetWeaponDefinitionAsString(header->rootKey, "");

    // Create exported path + asset path.
    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
    const std::filesystem::path wepnPath(asset->GetAssetName());

    if (g_ExportSettings.exportPathsFull)
        exportPath.append(wepnPath.parent_path().string());
    else
        exportPath.append("weapon/");

    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset directory.");
        return false;
    }

    exportPath.append(wepnPath.filename().string());
    exportPath.replace_extension(asJson ? ".json" : ".txt");

    StreamIO out;
    if (!out.open(exportPath.string(), eStreamIOMode::Write))
    {
        assertm(false, "Failed to open file for write.");
        return false;
    }

    out.write(wepnTxt.c_str(), wepnTxt.length());
    out.close();

    return false;
}

void InitWeaponDefinitionAssetType()
{
    static const char* settings[] = { "JSON", "TXT" };
    AssetTypeBinding_t type =
    {
        .name = "Weapon Definition",
        .type = 'npew',
        .headerAlignment = 8,
        .loadFunc = LoadWeaponDefinitionAsset,
        .postLoadFunc = nullptr,
        .previewFunc = nullptr,
        .e = { ExportWeaponDefinitionAsset, 0, settings, ARRSIZE(settings) },
    };

    REGISTER_TYPE(type);
}