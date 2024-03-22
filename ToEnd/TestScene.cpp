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
	if (node)
	{
		delete node;
	}
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();

	dxGraphic->LoadMeshDataFile("MeshData/meshes0.fbx", m_meshSet, &nodeData);

	/*TextureInfo texInfo;
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_MainTex.png");
	texInfo.blend = 1.0f;
	texInfo.uvIndex = 0;
	texInfo.type = aiTextureType_DIFFUSE;
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_Texture2.png");
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap.png");
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap2.png");
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_DetailMainTex.png");
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_OcclusionMap.png");
	m_textureBuffer->AddTexture(&texInfo);
	texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_NailMask.png");
	m_textureBuffer->AddTexture(&texInfo);*/
}

void TestScene::Update(float delta)
{

}

void TestScene::Render()
{

}
