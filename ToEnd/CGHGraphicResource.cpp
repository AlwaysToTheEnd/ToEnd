#include "CGHGraphicResource.h"

std::vector<std::string> TextureInfo::s_textureNames;
std::unordered_map<std::string, unsigned int> TextureInfo::s_textureIndex;

unsigned int TextureInfo::GetTextureFilePathID(const char* path)
{
	unsigned int result = 0;

	auto iter = s_textureIndex.find(path);

	if (iter == s_textureIndex.end())
	{
		result = static_cast<unsigned int>(s_textureIndex.size());
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
