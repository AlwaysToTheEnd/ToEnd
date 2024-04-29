#include "TestScene.h"
#include "GraphicDeivceDX12.h"
#include "DX12PipelineMG.h"
#include "CGHBaseClass.h"
#include "DirectXColors.h"
#include "DX12TextureBuffer.h"
#include "Xml.h"
#include <DirectXMath.h>
#include <ppl.h>
#include <iostream>
#include <fstream>
#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>
#include <DirectXColors.h>
#include "Dx12FontManager.h"
#include "GraphicResourceLoader.h"
#include "LightComponents.h"
#include "AnimationComponent.h"
#include "AnimationNodeNameFilter.h"

TestScene::TestScene()
{
}

TestScene::~TestScene()
{
	delete m_aniGroup;
}

void TestScene::Init()
{
	auto dxGraphic = GraphicDeviceDX12::GetGraphic();
	DX12FontManger::s_instance.LoadFont(L"FontData/baseFont.spritefont");

	dxGraphic->LoadMeshDataFile("MeshData/body0.fbx", true, &m_bodyMeshs, &m_bodyMats, &m_bodyNodes);
	{
		m_rootNode = &m_bodyNodes.front();

		auto render = m_rootNode->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = m_rootNode->CreateComponent<COMMaterial>();
		auto skinnedMesh = m_rootNode->CreateComponent<COMSkinnedMesh>();
		m_bodyMats.front().diffuse = { 0.9f, 0.72f, 0.65f };
		material->SetData(&m_bodyMats.front());
		skinnedMesh->SetMeshData(&m_bodyMeshs.front());
		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_BASE_COLOR;
		texInfo.textureOp = aiTextureOp_SignedAdd;
		material->SetTexture(&texInfo, 0);

		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_DetailMainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_BASE_COLOR;
		texInfo.textureOp = aiTextureOp_Subtract;
		material->SetTexture(&texInfo, 1);

		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.textureOp = aiTextureOp_Add;
		texInfo.type = aiTextureType_NORMAL_CAMERA;
		material->SetTexture(&texInfo, 2);

		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/BaseBody/cf_m_skin_body_00_BumpMap2.bmp");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.textureOp = aiTextureOp_Add;
		texInfo.type = aiTextureType_NORMAL_CAMERA;
		material->SetTexture(&texInfo, 3);
	}

	{
		dxGraphic->LoadMeshDataFile("MeshData/head0.fbx", true, &m_headMeshs, &m_headMats, &m_headNodes);
		std::string headRootName = "cf_J_Head_s";
		for (auto& iter : m_bodyNodes)
		{
			if (iter.GetName() == headRootName)
			{
				m_headNodes.front().SetParent(&iter, true);
				break;
			}
		}

		CGHNode* headRoot = &m_headNodes.front();

		auto render = headRoot->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = headRoot->CreateComponent<COMMaterial>();
		auto skinnedMesh = headRoot->CreateComponent<COMSkinnedMesh>();

		m_headMats.front().diffuse = { 0.9f, 0.72f, 0.65f };
		skinnedMesh->SetMeshData(&m_headMeshs.front());
		material->SetData(&m_headMats.front());

		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/cf_m_skin_head_01_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_BASE_COLOR;
		texInfo.textureOp = aiTextureOp_SignedAdd;
		material->SetTexture(&texInfo, 0);

		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/cf_m_skin_head_01_DetailMainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_BASE_COLOR;
		texInfo.textureOp = aiTextureOp_Subtract;
		texInfo.srcAlphaOp = TextureInfo::ALPHAOP_MULTIPLY;
		material->SetTexture(&texInfo, 1);


		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/cf_m_skin_head_01_BumpMap2_converted.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_NORMAL_CAMERA;
		texInfo.textureOp = aiTextureOp_Add;
		material->SetTexture(&texInfo, 2);
	}

	{
		dxGraphic->LoadMeshDataFile("MeshData/hair0.fbx", false, &m_hairMeshs, &m_hairMats, &m_hairNodes);

		std::string hairRootName = "cf_J_FaceUp_ty";
		for (auto& iter : m_headNodes)
		{
			if (iter.GetName() == hairRootName)
			{
				m_hairsRootNode.SetParent(&iter, true);
				m_hairNodes.front().SetParent(&m_hairsRootNode, true);
				m_hairsRootNode.CreateComponent<COMTransform>();
				m_hairsRootNode.SetName("N_hair_Root");
				break;
			}
		}

		CGHNode* hairRoot = &m_hairNodes.front();

		auto render = hairRoot->CreateComponent<COMDX12SkinnedMeshRenderer>();
		auto material = hairRoot->CreateComponent<COMMaterial>();
		auto skinnedMesh = hairRoot->CreateComponent<COMSkinnedMesh>();

		skinnedMesh->SetMeshData(&m_hairMeshs.front());
		material->SetData(&m_hairMats.front());

		TextureInfo texInfo;
		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/back_MainTex.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_BASE_COLOR;
		texInfo.textureOp = aiTextureOp_SignedAdd;
		material->SetTexture(&texInfo, 0);

		texInfo.textureFilePathID = TextureInfo::GetTextureFilePathID("Textures/baseHeadPart/back_BumpMap_converted.png");
		texInfo.blend = 1.0f;
		texInfo.uvIndex = 0;
		texInfo.type = aiTextureType_NORMAL_CAMERA;
		texInfo.textureOp = aiTextureOp_Subtract;
		material->SetTexture(&texInfo, 1);
	}

	{
		struct PosnScale
		{
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT3 scale;
		};
		std::unordered_map<std::string, PosnScale> nodeDatas;

		XmlDocument* document = new XmlDocument;
		document->LoadFile("MeshData/nodeTreeData.xml");
		assert(!document->Error());

		XmlElement* list = nullptr;
		list = document->FirstChildElement();
		list = list->FirstChildElement();

		for (; list != nullptr; list = list->NextSiblingElement())
		{
			PosnScale data;
			std::string name = list->Attribute("name");
			data.pos.x = -list->FloatAttribute("posX");
			data.pos.y = list->FloatAttribute("posY");
			data.pos.z = -list->FloatAttribute("posZ");

			data.scale.x = list->FloatAttribute("scaleX");
			data.scale.y = list->FloatAttribute("scaleY");
			data.scale.z = list->FloatAttribute("scaleZ");

			nodeDatas[name] = data;
		}

		std::vector<CGHNode*> nodeStack;
		m_rootNode->GetChildNodes(&nodeStack);

		for (auto& iter : nodeStack)
		{
			auto transform = iter->GetComponent<COMTransform>();
			auto nodeDataIter = nodeDatas.find(iter->GetName());

			if (nodeDataIter != nodeDatas.end())
			{
				transform->SetPos(DirectX::XMLoadFloat3(&nodeDataIter->second.pos));
				transform->SetScale(DirectX::XMLoadFloat3(&nodeDataIter->second.scale));
			}
		}

		delete document;
	}

	{
		auto light = m_dirLight.CreateComponent<COMDirLight>();
		light->m_data.color = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
		light->m_data.power = 1.0f;
		light->SetFlags(CGHLightComponent::LIGHT_FLAG_SHADOW);

		auto lightTransform = m_dirLight.CreateComponent<COMTransform>();
	}

	{
	/*	auto light = m_dirLight2.CreateComponent<COMDirLight>();
		light->m_data.color = DirectX::XMFLOAT3(0.0f, 0.3f, 1.0f);
		light->m_data.power = 1.0f;
		light->SetFlags(CGHLightComponent::LIGHT_FLAG_SHADOW);

		auto lightTransform = m_dirLight2.CreateComponent<COMTransform>();*/
	}

	auto fontrender = m_stringNode.CreateComponent<COMFontRenderer>();
	auto fontTrans = m_stringNode.CreateComponent<COMUITransform>();
	DirectX::XMVECTOR color = DirectX::Colors::Black;
	//color.m128_f32[3] = 0.5f;
	fontrender->SetRenderString(L"Test Rendering", color, 350.0f);
	fontTrans->SetPos(DirectX::XMVectorSet(10, 10, 0.1f, 0));
	fontTrans->SetSize(DirectX::XMVectorSet(1.0f, 1.0f, 0, 0));

	m_testButton.Init();
	m_testButton.AddFunc(0, 3, std::bind(&TestScene::ButtonTestFunc, this, std::placeholders::_1));
	m_testButton.SetSize(250, 30);
	m_testButton.SetPos(100, 100, 0.1f);
	color.m128_f32[3] = 0.5f;
	m_testButton.SetColor(color);
}

void TestScene::Update(float delta)
{
	auto graphic = GraphicDeviceDX12::GetGraphic();
	auto lightTrans = m_dirLight.GetComponent<COMTransform>();
	//auto lightTrans2 = m_dirLight2.GetComponent<COMTransform>();
	auto rootTrans = m_rootNode->GetComponent<COMTransform>();

	static float posY = 0.0f;
	//posY += delta * 1.0f;

	DirectX::XMFLOAT3 pos = { 0.0f,posY, 0.0f };
	rootTrans->SetPos(pos);

	static float x, y = 0;
	//x += delta * 1.0f;
	//y += delta * 0.8f;

	//auto fontrender = m_stringNode.GetComponent<COMFontRenderer>();
	std::wstring rotXvalue = L"curr rotY : " + std::to_wstring(int(y * 180 / 3.141592));
	//fontrender->SetText(rotXvalue.c_str());

	lightTrans->SetRotateQuter(DirectX::XMQuaternionRotationRollPitchYaw(x, y, 0));
	//lightTrans2->SetRotateQuter(DirectX::XMQuaternionRotationRollPitchYaw(y, x, 0));

	CGHRenderer::ExcuteMouseAction(graphic->GetCurrMouseTargetRenderID());
	m_rootNode->Update(graphic->GetCurrFrameIndex(), delta);
	m_dirLight.Update(graphic->GetCurrFrameIndex(), delta);
	m_dirLight2.Update(graphic->GetCurrFrameIndex(), delta);
	//m_stringNode.Update(graphic->GetCurrFrameIndex(), delta);
	m_testButton.Update(graphic->GetCurrFrameIndex(), delta);
}

void TestScene::RateUpdate(float delta)
{
	m_rootNode->RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
	m_dirLight.RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
	m_dirLight2.RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
	//m_stringNode.RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
	m_testButton.RateUpdate(GraphicDeviceDX12::GetGraphic()->GetCurrFrameIndex(), delta);
}

void TestScene::ButtonTestFunc(CGHNode* node)
{
	m_testButton.SetColor(DirectX::Colors::Blue);
}
