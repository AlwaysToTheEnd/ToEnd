#pragma once
#include <vector>
#include <DirectXMath.h>

struct CGHMeshDataSet;
struct CGHMaterialSet;


class TestScene
{
public:
	TestScene();
	~TestScene();

	void Init();
	void Update(float delta);
	void Render();

private:
	CGHMeshDataSet* m_meshSet;
	CGHMaterialSet* m_materialSet;
	std::vector<DirectX::XMFLOAT4X4> m_updatedBones;
};