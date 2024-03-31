#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "DirectXColors.h"
#include "DX12TextureBuffer.h"
#include "Xml.h"
#include <DirectXMath.h>
#include "LightComponents.h"

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();

	dxGraphic->LoadMeshDataFile("MeshData/body0.fbx", &m_bodyMeshs, &m_bodyMats, &m_bodyNodes);
	{
		m_rootNode = &m_bodyNodes.front();

		auto render = m_rootNode->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = m_rootNode->CreateComponent<COMMaterial>();
		auto skinnedMesh = m_rootNode->CreateComponent<COMSkinnedMesh>();

		skinnedMesh->SetMeshData(&m_bodyMeshs.front());
		material->SetData(&m_bodyMats.front());

		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_DIFFUSE;
		material->SetTexture(&texInfo, 0);
	}
	
	{
		dxGraphic->LoadMeshDataFile("MeshData/head0.fbx", &m_headMeshs, &m_headMats, &m_headNodes);
		std::string headRootName = "cf_J_Head_s";
		for (auto& iter : m_bodyNodes)
		{
			if (iter.GetaName() == headRootName)
			{
				m_headNodes.front().SetParent(&iter, true);
				break;
			}
		}

		CGHNode* headRoot = &m_headNodes.front();

		auto render = headRoot->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = headRoot->CreateComponent<COMMaterial>();
		auto skinnedMesh = headRoot->CreateComponent<COMSkinnedMesh>();

		skinnedMesh->SetMeshData(&m_headMeshs.front());
		material->SetData(&m_headMats.front());

		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/cf_m_skin_head_01_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_DIFFUSE;
		material->SetTexture(&texInfo, 0);
	}

	{
		dxGraphic->LoadMeshDataFile("MeshData/hair0.fbx", &m_hairMeshs, &m_hairMats, &m_hairNodes);
		std::string hairRootName = "cf_J_FaceUp_ty";
		for (auto& iter : m_headNodes)
		{
			if (iter.GetaName() == hairRootName)
			{
				m_hairNodes.front().SetParent(&iter, true);
				break;
			}
		}

		CGHNode* headRoot = &m_hairNodes.front();

		auto render = headRoot->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = headRoot->CreateComponent<COMMaterial>();
		auto skinnedMesh = headRoot->CreateComponent<COMSkinnedMesh>();

		skinnedMesh->SetMeshData(&m_hairMeshs.front());
		material->SetData(&m_hairMats.front());

		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/back_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_DIFFUSE;
		material->SetTexture(&texInfo, 0);
	}

	auto light = m_rootNode->CreateComponent<COMDirLight>();

	light->m_data.color = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
	light->m_data.power = 1.0f;
	light->m_data.dir = DirectX::XMFLOAT3(0, 1.0f, 0.5f);
}

void TestScene::Update(float delta)
{
	m_rootNode->Update(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
}

void TestScene::RateUpdate(float delta)
{
	m_rootNode->RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
}

void TestScene::Render()
{

}
