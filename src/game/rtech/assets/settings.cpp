#include <pch.h>
#include <game/rtech/assets/settings.h>
#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>

void LoadSettingsAsset(CAssetContainer* pak, CAsset* asset)
{
	UNUSED(pak);

	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	SettingsAsset* settingsAsset = nullptr;

	switch (pakAsset->data()->headerStructSize)
	{
	case 72:
	{
		SettingsAssetHeader_v1_t* hdr = reinterpret_cast<SettingsAssetHeader_v1_t*>(pakAsset->header());

		settingsAsset = new SettingsAsset(hdr);

		break;
	}
	case 80:
	{
		SettingsAssetHeader_v2_t* hdr = reinterpret_cast<SettingsAssetHeader_v2_t*>(pakAsset->header());

		settingsAsset = new SettingsAsset(hdr);

		break;
	}
	}

	if (settingsAsset)
	{
		pakAsset->SetAssetName(settingsAsset->name, true);
		pakAsset->setExtraData(settingsAsset);
	}
	// todo: error handling?
}

void PostLoadSettingsAsset(CAssetContainer* pak, CAsset* asset)
{
	UNUSED(pak);
	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	SettingsAsset* const settingsAsset = pakAsset->extraData<SettingsAsset* const>();

	settingsAsset->ParseSettingsData();
}

SettingsKVValue_t::~SettingsKVValue_t()
{
	if (numChildren > 0)
	{
		switch (type)
		{
		case eSettingsFieldType::ST_ARRAY:
		case eSettingsFieldType::ST_DYN_ARRAY:
		{
			printf("Deleting settings array\n");
			delete[] getValue<SettingsKVValue_t*>();
			break;
		}
		case eSettingsFieldType::_ST_OBJECT:
		{
			printf("Deleting settings object\n");
			delete[] getValue<SettingsKVField_t*>();
			break;
		}
		}
	}
}

struct DynamicArrayData_t
{
	int arraySize;
	int arrayOffset;
};

bool SettingsAsset::ParseSettingsData()
{
	// If the layout asset wasn't available when the load function was called, try again here.
	// If there still isn't a valid layout asset, return early
	if (!this->layoutAsset && !(this->layoutAsset = g_assetData.FindAssetByGUID<CPakAsset>(this->layoutGuid)))
			return false;

	if (_fields)
	{
		this->_numFields = 0;

		delete[] this->_fields;
		this->_fields = NULL;
	}

	const SettingsLayoutAsset* const layout = this->layoutAsset->extraData<const SettingsLayoutAsset* const>();

	this->_numFields = layout->layoutFields.size();
	this->_fields = new SettingsKVField_t[_numFields];

	for (size_t i = 0; i < _numFields; ++i)
	{
		SettingsKVField_t* field = &_fields[i];
		const SettingsField* const layoutField = &layout->layoutFields.at(i);

		field->key = layoutField->fieldName;

		R_ParseSettingsField(field, this->valueData, layout, layoutField);
	}

	return true;
}

void SettingsAsset::R_ParseSettingsField(SettingsKVField_t* field, const char* valData, const SettingsLayoutAsset* layout, const SettingsField* const layoutField)
{
	// Initially just copy across the type. Dynamic arrays will be collapsed down into regular arrays, since any functional difference
	// is removed when parsed
	field->value.type = layoutField->dataType;
	field->value.numChildren = 0;

	switch (layoutField->dataType)
	{
	case eSettingsFieldType::ST_BOOL:
	{
		field->value.value = static_cast<bool>(valData[layoutField->valueOffset]);
		break;
	}
	case eSettingsFieldType::ST_INTEGER:
	{
		field->value.value = *reinterpret_cast<const int*>(&valData[layoutField->valueOffset]);
		break;
	}
	case eSettingsFieldType::ST_FLOAT:
	{
		field->value.value = *reinterpret_cast<const float*>(&valData[layoutField->valueOffset]);
		break;
	}
	case eSettingsFieldType::ST_FLOAT2:
	{
		const float* floatValues = reinterpret_cast<const float*>(&valData[layoutField->valueOffset]);

		field->value.value = float2(floatValues[0], floatValues[1]);
		break;
	}
	case eSettingsFieldType::ST_FLOAT3:
	{
		const float* floatValues = reinterpret_cast<const float*>(&valData[layoutField->valueOffset]);

		field->value.value = float3(floatValues[0], floatValues[1], floatValues[2]);
		break;
	}
	case eSettingsFieldType::ST_STRING:
	case eSettingsFieldType::ST_ASSET:
	case eSettingsFieldType::ST_ASSET_NOPRECACHE:
	{
		const char* const charBuf = *(const char**)&valData[layoutField->valueOffset];
		field->value.value = charBuf;
		break;
	}
	case eSettingsFieldType::ST_ARRAY:
	{
		const SettingsLayoutAsset& subLayout = layout->subHeaders[layoutField->valueSubLayoutIdx];

		field->value.numChildren = subLayout.arrayValueCount;
		field->value.value = new SettingsKVValue_t[field->value.numChildren];

		R_ParseSettingsArray(field, &valData[layoutField->valueOffset], subLayout.arrayValueCount, subLayout);

		break;
	}
	case eSettingsFieldType::ST_DYN_ARRAY:
	{
		// When parsed, dynamic arrays and static arrays are functionally identical, so we might as well ditch the separate field types
		field->value.type = eSettingsFieldType::ST_ARRAY;

		const SettingsLayoutAsset& subLayout = layout->subHeaders[layoutField->valueSubLayoutIdx];
		const DynamicArrayData_t* dynamicArrayData = reinterpret_cast<const DynamicArrayData_t*>(&valData[layoutField->valueOffset]);

		const char* dynamicArrayElems = &valData[dynamicArrayData->arrayOffset];

		field->value.numChildren = dynamicArrayData->arraySize;

		field->value.value = new SettingsKVValue_t[field->value.numChildren];

		assert(std::get<SettingsKVValue_t*>(field->value.value));

		R_ParseSettingsArray(field, dynamicArrayElems, dynamicArrayData->arraySize, subLayout);

		break;
	}
	}
}

void SettingsAsset::R_ParseSettingsArray(SettingsKVField_t* field, const char* valData, const size_t arrayElemCount, const SettingsLayoutAsset& layout)
{
	// Total size of the values for one element of this array
	const size_t layoutSize = layout.totalLayoutSize;
	// Number of fields in each element of the array
	const size_t fieldCount = layout.layoutFields.size();

	for (size_t i = 0; i < arrayElemCount; ++i)
	{
		SettingsKVValue_t* arrayElem = &std::get<SettingsKVValue_t*>(field->value.value)[i];

		arrayElem->type = eSettingsFieldType::_ST_OBJECT;
		arrayElem->numChildren = static_cast<uint32_t>(fieldCount);
		arrayElem->value = new SettingsKVField_t[fieldCount];

		assert(std::get<SettingsKVField_t*>(arrayElem->value));

		const char* const elemValues = reinterpret_cast<const char*>(valData) + (i * layoutSize);

		for (size_t j = 0; j < fieldCount; ++j)
		{
			SettingsKVField_t* const subField = &std::get<SettingsKVField_t*>(arrayElem->value)[j];
			const SettingsField& layoutField = layout.layoutFields[j];

			subField->key = layoutField.fieldName;
			
			R_ParseSettingsField(subField, elemValues, &layout, &layoutField);
		}
	}
}

void SettingsAsset::R_WriteSetFileArray(std::string& out, const size_t indentLevel, const char* valuePtr,
	const size_t arrayElemCount, const SettingsLayoutAsset& subLayout)
{
	out += "[\n";

	const size_t layoutSize = subLayout.totalLayoutSize;
	const size_t fieldCount = subLayout.layoutFields.size();

	const std::string indentation = GetIndentation(indentLevel);

	for (size_t i = 0; i < arrayElemCount; ++i)
	{
		const char* const elemValues = reinterpret_cast<const char*>(valuePtr) + (i * layoutSize);
		out += indentation + "\t{\n";

		for (size_t j = 0; j < fieldCount; ++j)
		{
			const SettingsField& subField = subLayout.layoutFields[j];
			out += indentation + "\t\t" + "\"" + subField.fieldName + "\": ";

			R_WriteSetFile(out, indentLevel+2, elemValues, &subLayout , &subField);

			const char* const commaChar = j != (fieldCount - 1) ? ",\n" : "\n";
			out += commaChar;
		}

		out += indentation + "\t}";

		if (fieldCount)
		{
			const char* const commaChar = i != (arrayElemCount - 1) ? ",\n" : "\n";
			out += commaChar;
		}
	}

	out += indentation + "]";
}

void SettingsAsset::R_WriteSetFile(std::string& out, const size_t indentLevel, const char* valData,
	const SettingsLayoutAsset* layout, const SettingsField* const field)
{
	switch (field->dataType)
	{
	case eSettingsFieldType::ST_BOOL:
	{
		out.append((valData[field->valueOffset]) ? "true" : "false");
		break;
	}
	case eSettingsFieldType::ST_INTEGER:
	{
		out.append(std::format("{:d}", *reinterpret_cast<const int*>(&valData[field->valueOffset])));
		break;
	}
	case eSettingsFieldType::ST_FLOAT:
	{
		out.append(std::format("{:f}", *reinterpret_cast<const float*>(&valData[field->valueOffset])));
		break;
	}
	case eSettingsFieldType::ST_FLOAT2:
	{
		const float* floatValues = reinterpret_cast<const float*>(&valData[field->valueOffset]);
		out.append(std::format("\"<{:f},{:f}>\"", floatValues[0], floatValues[1]));

		break;
	}
	case eSettingsFieldType::ST_FLOAT3:
	{
		const float* floatValues = reinterpret_cast<const float*>(&valData[field->valueOffset]);
		out.append(std::format("\"<{:f},{:f},{:f}>\"", floatValues[0], floatValues[1], floatValues[2]));

		break;
	}
	case eSettingsFieldType::ST_STRING:
	case eSettingsFieldType::ST_ASSET:
	case eSettingsFieldType::ST_ASSET_NOPRECACHE:
	{
		const char* const charBuf = *(const char**)&valData[field->valueOffset];
		out.append(std::format("\"{:s}\"", charBuf));
		break;
	}
	case eSettingsFieldType::ST_ARRAY:
	{
		const SettingsLayoutAsset& subLayout = layout->subHeaders[field->valueSubLayoutIdx];
		R_WriteSetFileArray(out, indentLevel+1, &valData[field->valueOffset], subLayout.arrayValueCount, subLayout);

		break;
	}
	case eSettingsFieldType::ST_DYN_ARRAY:
	{
		const SettingsLayoutAsset& subLayout = layout->subHeaders[field->valueSubLayoutIdx];
		const DynamicArrayData_t* dynamicArrayData = reinterpret_cast<const DynamicArrayData_t*>(&valData[field->valueOffset]);

		const char* dynamicArrayElems = &valData[dynamicArrayData->arrayOffset];

		R_WriteSetFileArray(out, indentLevel+1, dynamicArrayElems, dynamicArrayData->arraySize, subLayout);

		break;
	}
	}
}

void SettingsAsset::R_WriteSetFile(std::string& out, const size_t indentLevel, const char* valData, const SettingsLayoutAsset* layout)
{
	const std::string indentation = GetIndentation(indentLevel);
	const size_t numLayoutFields = layout->layoutFields.size();

	for (size_t i = 0; i < numLayoutFields; ++i)
	{
		const SettingsField* const field = &layout->layoutFields.at(i);
		out += indentation + "\"" + field->fieldName + "\": ";

		R_WriteSetFile(out, indentLevel, valData, layout, field);

		const char* const commaChar = i != (numLayoutFields-1) ? ",\n" : "\n";
		out += commaChar;
	}
}

void SettingsAsset::R_WriteModNames(std::string& out) const
{
	out += "\t\"$modNames\": [\n";

	for (uint32_t i = 0; i < modNameCount; i++)
	{
		const char* const commaChar = i != (modNameCount - 1) ? "," : "";
		out += std::string("\t\t\"") + modNames[i] + "\"" + commaChar + "\n";
	}

	out += "\t]";
}

void SettingsAsset::R_WriteModValues(std::string& out, const SettingsLayoutAsset* const layout) const
{
	out += "\t\"$modValues\": [\n";

	for (uint32_t i = 0; i < modValuesCount; i++)
	{
		const SettingsMod_s* const modValue = &modValues[i];
		out += std::format("\t\t{{ // originally mapped to offset {:d}\n", modValue->valueOffset);

		out += std::format("\t\t\t\"index\": {:d},\n\t\t\t\"type\": \"{:s}\",\n\t\t\t",
			modValue->nameIndex, g_settingsModType[modValue->type]);

		SettingsLayoutFindByOffsetResult_s searchResult;
		const bool foundField = SettingsFieldFinder_FindFieldByAbsoluteOffset(layout, modValue->valueOffset, searchResult);

		if (foundField)
		{
			switch (modValue->type)
			{
			case SettingsModType_e::kIntAdd:
			case SettingsModType_e::kIntMultiply:
				out += std::format("\"value\": {:d},\n", modValue->value.intValue);
				break;
			case SettingsModType_e::kFloatAdd:
			case SettingsModType_e::kFloatMultiply:
				out += std::format("\"value\": {:f},\n", modValue->value.floatValue);
				break;
			case SettingsModType_e::kBool:
				out += std::format("\"value\": {:s},\n", modValue->value.boolValue ? "true" : "false");
				break;
			case SettingsModType_e::kNumber:
				if (searchResult.field->dataType == eSettingsFieldType::ST_INTEGER)
					out += std::format("\"value\": {:d},\n", modValue->value.intValue);
				else
					out += std::format("\"value\": {:f},\n", modValue->value.floatValue);
				break;
			case SettingsModType_e::kString:
				out += std::format("\"value\": \"{:s}\",\n", &stringData[modValue->value.stringOffset]);
				break;
			}

			out += std::format("\t\t\t\"field\": \"{:s}\"\n", searchResult.fieldAccessPath);
		}
		else
		{
			assert(0);
			out += "// FAILURE( !!! SETTINGS FIELD NOT FOUND !!! )\n";
		}

		const char* const commaChar = i != (modValuesCount - 1) ? "," : "";
		out += std::string("\t\t}") + commaChar + "\n";
	}

	out += "\t]";
}

static bool RenderSettingsAsset(CPakAsset* const asset, std::string& stringStream)
{
	SettingsAsset* settingsAsset = asset->extraData<SettingsAsset*>();
	if (!settingsAsset->layoutAsset)
		return false;

	const SettingsLayoutAsset* const layout = settingsAsset->layoutAsset->extraData<SettingsLayoutAsset*>();

	stringStream += std::string("{\n") + "\t\"layoutAsset\": \"" + layout->name + "\",\n";

	if (settingsAsset->uniqueId)
		stringStream += std::format("\t\"uniqueId\": {:d},\n", settingsAsset->uniqueId);

	stringStream += "\t\"settings\": {\n";

	// Recursively write the .set file contents into the string stream
	settingsAsset->R_WriteSetFile(stringStream, 2, (const char*)settingsAsset->valueData, layout);

	stringStream += "\t}";

	if (settingsAsset->modNameCount)
	{
		stringStream += ",\n";
		settingsAsset->R_WriteModNames(stringStream);
	}

	if (settingsAsset->modValuesCount)
	{
		stringStream += ",\n";
		settingsAsset->R_WriteModValues(stringStream, layout);
	}

	if (settingsAsset->modFlags)
		stringStream += std::format(",\n\t\"$modFlags\": {:d}\n", settingsAsset->modFlags);

	stringStream += "\n}";
	FixSlashes(stringStream);

	return true;
}

bool ExportSettingsAsset(CAsset* const asset, const int setting)
{
	UNUSED(setting);

	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	std::string stringStream;

	if (!RenderSettingsAsset(pakAsset, stringStream))
		return false;

	std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
	std::filesystem::path stgsPath = asset->GetAssetName();

	exportPath.append(stgsPath.parent_path().string());

	if (!CreateDirectories(exportPath))
	{
		assertm(false, "Failed to create asset type directory.");
		return false;
	}

	exportPath.append(stgsPath.filename().string());
	exportPath.replace_extension(".json");

	StreamIO out;
	if (!out.open(exportPath.string(), eStreamIOMode::Write))
	{
		assertm(false, "Failed to open file for write.");
		return false;
	}

	out.write(stringStream.c_str(), stringStream.length());
	return true;
}

static void* PreviewSettingsAsset(CAsset* const asset, const bool firstFrameForAsset)
{
	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	std::string stringStream;

	if (!RenderSettingsAsset(pakAsset, stringStream))
	{
		ImGui::Text("Settings asset unavailable");
		return nullptr;
	}

	UNUSED(firstFrameForAsset);
	ImGui::InputTextMultiline("##settings_preview", const_cast<char*>(stringStream.c_str()), stringStream.length()+1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);

	return nullptr;
}

void InitSettingsAssetType()
{
	AssetTypeBinding_t type =
	{
		.name = "Settings",
		.type = 'sgts',
		.headerAlignment = 8,
		.loadFunc = LoadSettingsAsset,
		.postLoadFunc = PostLoadSettingsAsset,
		.previewFunc = PreviewSettingsAsset,
		.e = { ExportSettingsAsset, 0, nullptr, 0ull },
	};

	REGISTER_TYPE(type);
}