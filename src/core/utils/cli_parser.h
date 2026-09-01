#pragma once

#include <cassert>
#include <cstring>
#include <vector>

class CCommandLine
{
public:
	CCommandLine(const int argc, char* argv[]) : argc(argc), argv(argv)
	{
	};

	const char* const GetSelfPath() const
	{
		assert(argc > 0);
		return argv[0];
	}

	const bool HasParam(const char* const param) const
	{
		assert(param);
		for (int i = 1; i < argc; ++i)
		{
			if (strcmp(argv[i], param) == 0)
			{
				return true;
			}
		}

		return false;
	}

	const int GetParamIdx(const char* const param) const
	{
		assert(param);
		for (int i = 1; i < argc; ++i)
		{
			if (strcmp(argv[i], param) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	const char* const GetParamValue(const char* const param) const
	{
		assert(param);
		const int idx = GetParamIdx(param);
		return (idx != -1 && idx != argc-1) ? GetParamValue(idx+1) : nullptr;
	}

	const char* const GetParamValue(const int idx) const
	{
		if (idx < 0 || idx >= argc)
		{
			assert(false && "range check failure");
			return nullptr;
		}

		return argv[idx];
	}

	inline const int GetArgC() const
	{
		return argc;
	}

	inline const int GetFirstNonFlagArgIdx() const
	{
		int i = 1;
		// Skip the first argument, as it's always our own executable path
		for (i = 1; i < argc; ++i)
		{
			// If the arg starts with a dash, then we need to keep looking
			if (argv[i][0] == '-')
			{
				// If the arg starts with TWO dashes, we should skip an extra arg as long as the subsequent conditions hold true
				if (strlen(argv[i]) > 1 && argv[i][1] == '-')
				{
					// If this arg is not the final arg, then check if the next arg starts with a dash
					// If the next arg does start with a dash, then we should not skip the extra arg here.
					if (i != argc - 1 && argv[i + 1][0] != '-')
						i++;
				}
			}
			else // No dash: we've found the first non-flag argument
				break;
		}
		return i;
	}

private:
	int argc;
	char** argv;
};

std::vector<uint32_t> GetExportFilterTypes(const CCommandLine* const cli);