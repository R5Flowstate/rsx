#pragma once

// S30/S31 (pak version 20/21) studiohdr -> v17 bytes.
bool ConvertRMDL19S30To17(const char* pIn, size_t inSize, std::vector<char>& outBytes,
	std::vector<std::string>& warnings, std::string& err);

// Stored S30/S31 vertex slice (per-group Oodle) -> raw v17 vertex buffer.
bool ConvertVGStored19S30(const char* pRmdl, size_t rmdlSize, const char* pStored,
	size_t storedSize, std::vector<char>& outRaw, std::string& err);
