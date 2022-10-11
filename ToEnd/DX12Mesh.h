#pragma once
#include <string>
#include <memory>
#include <wrl.h>
#include <DirectXMath.h>

class GraphicDeviceDX12;
class ID3D12Resource;
class ID3D12CommandSignature;

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
	static void DX12MeshBaseDataInit();

	virtual void Render();

private:
	static void* s_meshPipelinestate;
	static void* s_commandSignature;

protected:
	ID3D12Resource* m_vertexBuffer;
	ID3D12Resource* m_indexBuffer;
	void*			m_InputElementDescs;
};

