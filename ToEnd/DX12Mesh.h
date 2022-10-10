#pragma once
#include <string>
#include <memory>
#include <wrl.h>
#include <DirectXMath.h>

class GraphicDeviceDX12;
class ID3D12Resource;

struct Material
{
	DirectX::XMFLOAT4	diffuseAlbedo = { 1,1,1,1 };
	DirectX::XMFLOAT3	specular = { 0.01f,0.01f,0.01f };
	float				specularExponent = 0.25f;
	DirectX::XMFLOAT3	emissive = { 0,0,0 };
	float				pad0;
};

class DX12Mesh
{
public:
	DX12Mesh();
	virtual ~DX12Mesh();

	virtual void CreateMesh(const wchar_t* filePath, const wchar_t* meshName);
	virtual void Render();

private:
	ID3D12Resource* m_vertexBuffer;
	ID3D12Resource* m_indexBuffer;
	void*			m_InputElementDescs;

};

