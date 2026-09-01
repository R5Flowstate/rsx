// This file is probably in the wrong place; I'll figure it out later.

#include <pch.h>
#include <imgui.h>
#include <game/asset.h>
#include "core/render/uistate.h"
#include <game/rtech/cpakfile.h>
#include <game/rtech/assets/settings.h>
#include <core/render/dx.h>
#include <game/rtech/assets/localization.h>

extern CDXParentHandler* g_dxHandler;

void ItemflavWindow_GetCharacterDataFromSettings(CAsset* asset)
{
    CUIState& uiState = g_dxHandler->GetUIState();

    CUI_ItemflavCharacter* character = nullptr;

    int characterIdx = 0;

    for (characterIdx = 0; characterIdx < uiState.itemflavData.numCharacters; ++characterIdx)
    {
        if (uiState.itemflavData.characterData[characterIdx].settingsAssetGuid == asset->GetAssetGUID())
        {
            character = &uiState.itemflavData.characterData[characterIdx];
            break;
        }
    }

    assertm(character, "uh oh");

    character->settingsAsset = asset;

    SettingsAsset* const settingsAsset = reinterpret_cast<SettingsAsset*>(reinterpret_cast<CPakAsset*>(asset)->extraData());

    if (settingsAsset->_numFields == 0)
        settingsAsset->ParseSettingsData();
    
    SettingsKVValue_t* value = nullptr;

    if (settingsAsset->GetSettingValue("localizationKey_NAME", &value))
        character->characterName = value->getValue<const char*>();

    if(settingsAsset->GetSettingValue("localizationKey_DESCRIPTION_SHORT", &value))
        character->characterDesc = value->getValue<const char*>();

    if (settingsAsset->GetSettingValue("shippingStatus", &value))
        character->shippingStatus = value->getValue<const char*>();

    if (settingsAsset->GetSettingValue("skins", &value))
    {
        assert(value->type == eSettingsFieldType::ST_ARRAY);

        character->numSkins = value->numChildren;
        character->skins = new CUI_ItemflavCharacterSkin[character->numSkins];

        for (uint32_t j = 0; j < character->numSkins; ++j)
        {
            SettingsKVValue_t* const skinObj = &value->getValue<SettingsKVValue_t*>()[j];

            CUI_ItemflavCharacterSkin* const skin = &character->skins[j];

            // first field in the object is always the settings asset path
            skin->assetPath = skinObj->getValue<SettingsKVField_t*>()[0].getValue<const char*>();
            skin->settingsAsset = g_assetData.FindAssetByGUID(RTech::StringToGuid(skin->assetPath));
        }
    }
}

void ItemflavWindow_LocalizationPostLoadCallback(CAsset* asset)
{
    CUIState& uiState = g_dxHandler->GetUIState();

    uiState.itemflavData.localizationAsset = asset;
}

std::string Localize(const char* key)
{
    const char* origKey = key;

    if (strnlen(origKey, 1024) <= 1)
        return origKey;

    // skip over the 
    if (key[0] == '#')
        key++;

    CUIState& uiState = g_dxHandler->GetUIState();

    if (uiState.itemflavData.localizationAsset)
    {
        const LocalizationAsset* loc = reinterpret_cast<CPakAsset*>(uiState.itemflavData.localizationAsset)->extraData<LocalizationAsset*>();
        
        if (auto it = loc->entryMap.find(RTech::StringToGuid(key)); it != loc->entryMap.end())
        {
                return it->second;
        }
    }

    return origKey;
}

void ItemflavWindow_RefreshData(CUIState* uiState)
{
    CUI_ItemflavWindowData* const flavData = &uiState->itemflavData;

    uiState->itemFlavorListAsset = g_assetData.FindAssetByGUID(RTech::StringToGuid("settings\\base_itemflavors.rpak"));

    // If the list asset has now been found, populate the character list
    if (uiState->itemFlavorListAsset)
    {
        const uint64_t localizationGuid = RTech::StringToGuid("localization\\localization_english.rpak");
        if (CAsset* localizationAsset = g_assetData.FindAssetByGUID(localizationGuid))
            ItemflavWindow_LocalizationPostLoadCallback(localizationAsset);
        else
        {
            // Full pak path for the localization_english.rpak file in the same directory as common.rpak
            const std::filesystem::path fullPakPath = reinterpret_cast<CPakFile*>(reinterpret_cast<CPakAsset*>(uiState->itemFlavorListAsset)->GetContainerFile())->GetFilePath().parent_path() / "localization_english.rpak";

            extern void HandlePakLoad(std::vector<std::string> filePaths);

            HandlePakLoad({ fullPakPath.string() });

            g_assetData.AddAssetPostLoadCallback(localizationGuid, ItemflavWindow_LocalizationPostLoadCallback);
        }

        CPakAsset* const itemflavListAsset = reinterpret_cast<CPakAsset*>(uiState->itemFlavorListAsset);
        const SettingsAsset* const itemflavList = itemflavListAsset->extraData<SettingsAsset*>();

        SettingsKVValue_t* value = nullptr;
        if (itemflavList->GetSettingValue("characters", &value))
        {
            const uint32_t numCharacters = value->numChildren;
            const SettingsKVValue_t* const arrayElems = value->getValue<SettingsKVValue_t*>();

            flavData->AllocCharacterData(numCharacters);

            for (uint32_t j = 0; j < numCharacters; ++j)
            {
                const SettingsKVField_t* const elemFields = arrayElems[j].getValue<SettingsKVField_t*>();

                assert(arrayElems[j].numChildren > 0);
                if (arrayElems[j].numChildren == 0)
                    break;

                CUI_ItemflavCharacter* const character = &flavData->characterData[j];

                character->assetPath = elemFields[0].getValue<const char*>();
                character->settingsAssetGuid = RTech::StringToGuid(character->assetPath);

                // If the settings asset is already loaded, try and populate the data now
                if (CAsset* settingsAsset = g_assetData.FindAssetByGUID(character->settingsAssetGuid))
                    ItemflavWindow_GetCharacterDataFromSettings(settingsAsset);
                else
                    g_assetData.AddAssetPostLoadCallback(character->settingsAssetGuid, ItemflavWindow_GetCharacterDataFromSettings);
            }
        }
    }
}

void ItemflavWindow_Draw(CUIState* uiState)
{
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_Always);
    if (ImGui::Begin("Itemflavors", &uiState->itemflavWindowVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("This menu contains a list of all registered cosmetic items associated with each game character.\n"
            "For all features to work properly, RSX must load common.rpak, common_early.rpak, and your chosen localization rpak.");

        CUI_ItemflavWindowData* flavData = &uiState->itemflavData;

        // Always render the refresh button, but also try to grab everything the first time the window is shown
        if ((ImGui::Button("Refresh data") || !flavData->triedToInitialise) && g_assetData.m_donePostLoad)
        {
            ItemflavWindow_RefreshData(uiState);

            flavData->triedToInitialise = true;
        }

        if (flavData->localizationAsset)
        {
            if (ImGui::BeginCombo("##characters", flavData->selectedCharacterName.c_str()))
            {
                for (int i = 0; i < flavData->numCharacters; ++i)
                {
                    CUI_ItemflavCharacter* const character = &flavData->characterData[i];
                    bool isSelected = i == uiState->itemflavData.selectedCharacterIdx;

                    const std::string charName = Localize(character->characterName);

                    if (ImGui::Selectable(charName.c_str(), &isSelected))
                    {
                        flavData->selectedCharacterIdx = i;
                        flavData->selectedCharacterName = charName;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (flavData->selectedCharacterIdx != -1)
            {
                CUI_ItemflavCharacter* character = &flavData->characterData[flavData->selectedCharacterIdx];
                ImGui::Text("%s", Localize(character->characterDesc).c_str());

                ImGui::Text("Skins:");
                for (uint32_t i = 0; i < character->numSkins; ++i)
                {
                    SettingsAsset* const skinSettings = reinterpret_cast<CPakAsset*>(character->skins[i].settingsAsset)->extraData<SettingsAsset*>();

                    SettingsKVValue_t* skinNameValue = nullptr;
                    assertm(skinSettings->GetSettingValue("localizationKey_NAME", &skinNameValue), "Failed to get skin name");

                    SettingsKVValue_t* qualityValue = nullptr;
                    assertm(skinSettings->GetSettingValue("quality", &qualityValue), "Failed to get skin quality");

                    SettingsKVValue_t* bodyModelValue = nullptr;
                    assertm(skinSettings->GetSettingValue("bodyModel", &bodyModelValue), "Failed to get skin quality");

                    SettingsKVValue_t* bodyModelOdlValue = nullptr;
                    assertm(skinSettings->GetSettingValue("bodyModel_odlBlob", &bodyModelOdlValue), "Failed to get skin quality");

                    const std::string skinName = Localize(skinNameValue->getValue<const char*>());
                    ImGui::Text("\t%s: %s\n\t%s (%s)", qualityValue->getValue<const char*>(), skinName.c_str(), bodyModelValue->getValue<const char*>(), bodyModelOdlValue->getValue<const char*>());
                }
            }
        }

        ImGui::End();
    }
}
