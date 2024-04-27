#pragma once
#include <string>
#include <vector>
#include <algorithm>

const static std::string s_LRTokens[] = { "LEFT", "RIGHT" };


// leftRightOut = -1 (left), leftRightOut = 0 (none), leftRightOut = 1 (right)
// indexOut = -1 : not has Index
inline void FiltAniNodeName(const char* nodeName, const std::vector<std::string>& filters, std::string& filtedNameOut, char& leftRightOut, int& indexOut, int indexOffset = 0)
{
	std::string capitalName = nodeName;

	std::transform(capitalName.begin(), capitalName.end(), capitalName.begin(), [](char c)
		{
			return std::toupper(c);
		});

	leftRightOut = 0;

	for (int i = 0; i < _countof(s_LRTokens); i++)
	{
		size_t result = capitalName.find(s_LRTokens[i]);

		if (result != std::string::npos)
		{
			leftRightOut = i % 2 ? 1 : -1;
			capitalName = capitalName.substr(0, result) + capitalName.substr(result + s_LRTokens[i].size());
			break;
		}
	}

	if (capitalName.size() && leftRightOut == 0)
	{
		unsigned int lastIndex = capitalName.size() - 1;
		char LRToken[2] = { 'L', 'R' };

		for (int i = 0; i < _countof(LRToken); i++)
		{
			size_t currIndex = capitalName.find_first_of(LRToken[i]);

			while (currIndex != std::string::npos)
			{
				if (currIndex == lastIndex)
				{
					if (currIndex > 0)
					{
						if (capitalName[currIndex - 1] == '_')
						{
							leftRightOut = i % 2 ? 1 : -1;
							capitalName = capitalName.substr(0, currIndex - 1) + capitalName.substr(currIndex + 1);
							break;
						}
					}
				}
				else
				{
					if (currIndex + 1 < capitalName.size() && currIndex > 0)
					{
						if (capitalName[currIndex + 1] == '_' && capitalName[currIndex - 1] == '_')
						{
							leftRightOut = i % 2 ? 1 : -1;
							capitalName = capitalName.substr(0, currIndex - 1) + capitalName.substr(currIndex + 1);
							break;
						}
					}
				}

				currIndex = capitalName.find_first_of(LRToken[i], currIndex + 1);
			}

		}
	}

	for (auto& iter : filters)
	{
		size_t result = capitalName.find(iter);

		if (result != std::string::npos)
		{
			capitalName = capitalName.substr(0, result) + capitalName.substr(result + iter.size());
		}
	}

	std::string withoutIndex;
	std::string currIndex;
	std::vector<unsigned int> indexStack;
	for (auto& iter : capitalName)
	{
		if (iter >= '0' && iter <= '9')
		{
			currIndex += iter;
		}
		else
		{
			withoutIndex += iter;
			if (currIndex.size())
			{
				indexStack.push_back(std::stoi(currIndex));
				currIndex.clear();
			}
		}
	}

	if (currIndex.size())
	{
		indexStack.push_back(std::stoi(currIndex));
	}

	filtedNameOut = withoutIndex;

	indexOut = -1;

	if (indexStack.size())
	{
		indexOut = indexStack.back() + indexOffset;
	}
}

