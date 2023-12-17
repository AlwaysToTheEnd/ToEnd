#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "CGHGraphicResource.h"

TestScene::TestScene()
	: m_meshSet(nullptr)
	, m_materialSet(nullptr)
{

}

TestScene::~TestScene()
{
	if (m_meshSet)
	{
		delete m_meshSet;
	}

	if (m_materialSet)
	{
		delete m_materialSet;
	}
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();
	auto dxDevice = dxGraphic->GetDevice();

	m_meshSet = new CGHMeshDataSet;
	m_materialSet = new CGHMaterialSet;

	dxGraphic->LoadMeshDataFile("./../Common/MeshData/pants.fbx", m_meshSet, m_materialSet);

}

void TestScene::Update(float delta)
{
}

void TestScene::Render()
{
}
