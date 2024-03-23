#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "DirectXColors.h"
#include "DX12TextureBuffer.h"
#include "Xml.h"
#include <DirectXMath.h>

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
}

void TestScene::Init()
{
	/*{
		XmlDocument* document = new XmlDocument;
		std::string errorName;
		document->LoadFile("MeshData/nodeTreeData.xml");
		if (document->Error())
		{
			errorName = document->ErrorIDToName(document->ErrorID());

		}

		std::vector<int> parentIndex;
		parentIndex.reserve(1024);
		m_nodes.reserve(1024);
		XmlElement* list = document->FirstChildElement();

		DirectX::XMFLOAT3 pos = {};
		DirectX::XMFLOAT3 scale = {};
		DirectX::XMFLOAT4 rotation = {};

		for (list = list->FirstChildElement(); list != nullptr; list = list->NextSiblingElement())
		{
			m_nodes.emplace_back();
			auto transform = m_nodes.back().CreateComponent<COMTransform>();

			m_nodes.back().SetName(list->Attribute("name"));
			pos.x = list->FloatAttribute("posX");
			pos.y = list->FloatAttribute("posY");
			pos.z = list->FloatAttribute("posZ");

			scale.x = list->FloatAttribute("scaleX");
			scale.y = list->FloatAttribute("scaleY");
			scale.z = list->FloatAttribute("scaleZ");

			rotation.x = list->FloatAttribute("rotationX");
			rotation.y = list->FloatAttribute("rotationY");
			rotation.z = list->FloatAttribute("rotationZ");
			rotation.w = list->FloatAttribute("rotationW");

			transform->SetPos(DirectX::XMLoadFloat3(&pos));
			transform->SetScale(DirectX::XMLoadFloat3(&scale));
			transform->SetRotateQuter(DirectX::XMLoadFloat4(&rotation));

			int parentIndex = list->IntAttribute("pIndex");

			if (parentIndex != -1)
			{
				m_nodes.back().SetParent(&m_nodes[parentIndex]);
			}
		}

		delete document;
	}*/
	
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();
	
	dxGraphic->LoadMeshDataFile("MeshData/meshes0.fbx", &m_bodyMeshs, &m_bodyMats, &m_bodyNodes);
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
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_Texture2.png");
	//m_textureBuffer->AddTexture(&texInfo);
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap.png");
	//m_textureBuffer->AddTexture(&texInfo);
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap2.png");
	//m_textureBuffer->AddTexture(&texInfo);
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_DetailMainTex.png");
	//m_textureBuffer->AddTexture(&texInfo);
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_OcclusionMap.png");
	//m_textureBuffer->AddTexture(&texInfo);
	//texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_NailMask.png");
	//m_textureBuffer->AddTexture(&texInfo);
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
