#include <pch.h>
#include <core/utils/exportsettings.h>
#include <game/asset.h>

void ExportSettings_t::SetFromCLI(const CCommandLine* cli)
{
	if (const char* const nmlRecalc = cli->GetParamValue("--nmlrecalc"))
	{
		if (!_stricmp(nmlRecalc, "none"))
			this->exportNormalRecalcSetting = NML_RECALC_NONE;
		else if (!_stricmp(nmlRecalc, "directx"))
			this->exportNormalRecalcSetting = NML_RECALC_DX;
		else if (!_stricmp(nmlRecalc, "opengl"))
			this->exportNormalRecalcSetting = NML_RECALC_OGL;
	}

	if (const char* const texNames = cli->GetParamValue("--texturenames"))
	{
		if (!_stricmp(texNames, "guid"))
			this->exportTextureNameSetting = TXTR_NAME_GUID;
		else if (!_stricmp(texNames, "stored"))
			this->exportTextureNameSetting = TXTR_NAME_REAL;
		else if (!_stricmp(texNames, "text"))
			this->exportTextureNameSetting = TXTR_NAME_TEXT;
		else if (!_stricmp(texNames, "semantic"))
			this->exportTextureNameSetting = TXTR_NAME_SMTC;
	}

	this->exportMaterialTextures = cli->HasParam("-matltextures");
	this->exportPathsFull = cli->HasParam("-exportfullpaths");
	this->shaderKeepSM51 = cli->HasParam("-keepsm51");
	this->exportAssetDeps = cli->HasParam("-exportdependencies");
	this->disableCachedNames = cli->HasParam("-nocachedb");

	if (const char* const qcMajorStr = cli->GetParamValue("--qcmajor"))
		this->qcMajorVersion = static_cast<uint16_t>(atoi(qcMajorStr));

	if (const char* const qcMinorStr = cli->GetParamValue("--qcminor"))
		this->qcMinorVersion = static_cast<uint16_t>(atoi(qcMinorStr));

	this->exportRigSequences = cli->HasParam("-exportrigsequences");
	this->exportModelSkin = false; // todo: maybe make an option to replace this for exporting all skins, since skins cant be picked on CLI
	this->exportModelMatsTruncated = cli->HasParam("-truncatemodelmats");
	this->exportQCIFiles = cli->HasParam("-useqci");

	// i'm not too happy with this being "--exportdir", so this may change at some point
	if (const char* const exportPath = cli->GetParamValue("--exportdir"))
		this->exportDirectory = exportPath;

	// --exportsetting <type>=<n> (repeatable): override the code default of an
	// asset type's export setting, e.g. --exportsetting uiia=0 for PNG (HQ).
	// Parsed here, applied in ApplyExportSettingOverrides once the asset type
	// bindings exist. CCommandLine only returns the first value for a param,
	// so scan argv directly to support repeats.
	for (int i = 1; i < cli->GetArgC(); ++i)
	{
		if (strcmp(cli->GetParamValue(i), "--exportsetting") != 0)
			continue;

		if (i + 1 >= cli->GetArgC())
		{
			printf("EXPORT: --exportsetting missing value; ignoring.\n");
			continue;
		}

		const char* const val = cli->GetParamValue(i + 1);
		const char* const eq = strchr(val, '=');
		if (!eq || eq == val || eq - val > 4 || *(eq + 1) == '\0')
		{
			printf("EXPORT: --exportsetting malformed value '%s'; want <type>=<n> (e.g. uiia=0).\n", val);
			continue;
		}

		const size_t typeLen = static_cast<size_t>(eq - val);
		const char a = val[0];
		const char b = typeLen >= 2 ? val[1] : '\0';
		const char c = typeLen >= 3 ? val[2] : '\0';
		const char d = typeLen >= 4 ? val[3] : '\0';

		this->exportSettingOverrides.emplace_back(MAKEFOURCC(a, b, c, d), atoi(eq + 1));
	}
}

void ExportSettings_t::ApplyExportSettingOverrides()
{
	for (const auto& [type, setting] : this->exportSettingOverrides)
	{
		const auto it = g_assetData.m_assetTypeBindings.find(type);
		if (it == g_assetData.m_assetTypeBindings.end())
		{
			printf("EXPORT: --exportsetting unknown asset type '%.4s'; ignoring.\n", reinterpret_cast<const char*>(&type));
			continue;
		}

		AssetTypeBinding_t& binding = it->second;
		if (setting < 0 || static_cast<size_t>(setting) >= binding.e.exportSettingArrSize)
		{
			printf("EXPORT: --exportsetting index %d out of range for '%.4s' (0-%llu); ignoring.\n",
				setting, reinterpret_cast<const char*>(&type),
				static_cast<unsigned long long>(binding.e.exportSettingArrSize ? binding.e.exportSettingArrSize - 1 : 0));
			continue;
		}

		binding.e.exportSetting = setting;
		printf("EXPORT: --exportsetting '%.4s' -> '%s' (%d).\n",
			reinterpret_cast<const char*>(&type), binding.e.exportSettingArr[setting], setting);
	}
}