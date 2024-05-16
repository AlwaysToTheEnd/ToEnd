#include "NodeTransformController.h"
#include "CGHNodePicker.h"

NodeTransformController::NodeTransformController()
{
}

NodeTransformController::~NodeTransformController()
{
}

void NodeTransformController::Init()
{
	m_uiTransform = CreateComponent<COMUITransform>();
	m_backRender = CreateComponent<COMUIRenderer>();

}

void NodeTransformController::Update(float delta)
{
	CGHNode* currTarget = CGHNodePicker::s_instance.GetCurrPickedNode(CGHNODE_LAYER::CGHNODE_LYAER_CHARACTER);

	if (m_prevTarget != currTarget)
	{
		m_currNodeTree.clear();
		m_currTransforms.clear();

		if (currTarget)
		{
			currTarget = currTarget->GetRootNode();

			currTarget->GetChildNodes(&m_currNodeTree);

			for (auto iter : m_currNodeTree)
			{
				m_currTransforms.emplace_back(iter->GetComponent<COMTransform>());
			}
		}
	}

	if (m_currNodeTree.size())
	{
		m_currButtonIndex = 0;



	}

	m_prevTarget = currTarget;
}

void NodeTransformController::RateUpdate(float delta)
{

}

void NodeTransformController::SetSize(unsigned int x, unsigned int y)
{
	m_uiTransform->SetSize(DirectX::XMVectorSet(x, y, 0, 0));
}

void NodeTransformController::SetPos(unsigned int x, unsigned int y, float z)
{
	m_uiTransform->SetPos(DirectX::XMVectorSet(x, y, z, 0));
}