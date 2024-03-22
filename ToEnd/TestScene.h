#pragma once
#include <vector>
#include <unordered_map>
#include "CGHBaseClass.h"

class TestScene
{
public:
	TestScene();
	~TestScene();

	void Init();
	void Update(float delta);
	void RateUpdate(float delta);
	void Render();

private:
	CGHNode* node = nullptr;
	std::vector<CGHNode> m_nodes;
	std::vector<CGHMaterial> m_mats;
	std::vector<CGHMesh> m_meshes;
	CGHNode* m_rootNode = nullptr;
};