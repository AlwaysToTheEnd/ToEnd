#include "CGHGraphicResource.h"

std::vector<std::string> TextureInfo::s_textureNames;
std::unordered_map<std::string, size_t> TextureInfo::s_textureIndex;

size_t TextureInfo::GetTextureFilePathID(const char* path)
{
	size_t result = 0;

	auto iter = s_textureIndex.find(path);

	if (iter == s_textureIndex.end())
	{
		result = s_textureIndex.size();
		s_textureNames.push_back(path);
		s_textureIndex[path] = result;
	}
	else
	{
		result = iter->second;
	}

	return result;
}

const std::string& TextureInfo::GetTexturePath(unsigned int pathID)
{
	assert(s_textureNames.size() > pathID);

	return s_textureNames[pathID];
}
