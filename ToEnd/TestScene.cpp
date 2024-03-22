#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "DirectXColors.h"
#include "DX12TextureBuffer.h"

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
	
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();

	dxGraphic->LoadMeshDataFile("MeshData/meshes0.fbx", &m_meshes, &m_mats, &m_nodes);
	m_rootNode = &m_nodes.front();

	auto render = m_rootNode->CreateComponent<COMDX12SkinnedMeshRenderer>();
	auto material = m_rootNode->CreateComponent<COMMaterial>();
	auto skinnedMesh = m_rootNode->CreateComponent<COMSkinnedMesh>();

	skinnedMesh->SetMeshData(&m_meshes.front());
	material->SetData(&m_mats.front());

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
