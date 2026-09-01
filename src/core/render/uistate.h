#pragma once

struct CUI_ItemflavCharacterSkin
{
	const char* assetPath;

	void* settingsAsset;
};

struct CUI_ItemflavCharacter
{
	const char* assetPath;

	uint64_t settingsAssetGuid;
	void* settingsAsset;

	const char* characterName;
	const char* characterDesc;
	const char* shippingStatus;

	uint32_t numSkins;
	CUI_ItemflavCharacterSkin* skins;

};

struct CUI_ItemflavWindowData
{
	bool triedToInitialise;

	int numCharacters;
	CUI_ItemflavCharacter* characterData;

	void* localizationAsset;

	std::string selectedCharacterName;
	int selectedCharacterIdx;

	void Reset()
	{
		triedToInitialise = false;
		numCharacters = 0;
		
		if (characterData)
			delete[] characterData;

		characterData = nullptr;

		localizationAsset = nullptr;
		selectedCharacterName = "(none)";
		selectedCharacterIdx = -1;
	}

	void AllocCharacterData(int numChars)
	{
		this->numCharacters = numChars;
		this->characterData = new CUI_ItemflavCharacter[numChars];
	}
};

class CUIState
{
public:
	inline void ShowSettingsWindow(bool state) { settingsWindowVisible = state; };
	inline void ShowItemflavWindow(bool state) { itemflavWindowVisible = state; };
	inline void ShowLogWindow(bool state) { logWindowVisible = state; };
	inline void ShowTexturePreviewWindow(bool state) { texturePreviewWindowVisible = state; };

	inline void ClearAssetData()
	{
		itemFlavorListAsset = nullptr;

		itemflavData.Reset();
	}

public:
	bool settingsWindowVisible = false;
	bool itemflavWindowVisible = false;
	bool logWindowVisible = false;
	bool texturePreviewWindowVisible = false;

	void* itemFlavorListAsset = nullptr;

	CUI_ItemflavWindowData itemflavData;
};