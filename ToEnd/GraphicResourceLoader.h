#pragma once
#include <string>

struct ID3D12GraphicsCommandList;

class DX12GraphicResourceLoader
{
public:
	void LoadAllData(const std::string& filePath, ID3D12GraphicsCommandList* cmd);

private:

};

