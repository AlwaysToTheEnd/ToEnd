#pragma once
#include <vector>
#include <unordered_map>
#include "CGHBaseClass.h"
#include "BaseComponents.h"

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
	std::vector<CGHNode> m_headNodes;
	std::vector<CGHMesh> m_headMeshs;
	std::vector<CGHMaterial> m_headMats;

	std::vector<CGHNode> m_hairNodes;
	std::vector<CGHMesh> m_hairMeshs;
	std::vector<CGHMaterial> m_hairMats;

	std::vector<CGHNode> m_bodyNodes;
	std::vector<CGHMesh> m_bodyMeshs;
	std::vector<CGHMaterial> m_bodyMats;
	CGHNode* m_rootNode = nullptr;
	CGHNode m_dirLight;
};