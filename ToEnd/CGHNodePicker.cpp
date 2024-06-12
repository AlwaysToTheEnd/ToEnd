#include "CGHNodePicker.h"
#include "BaseComponents.h"

CGHNodePicker CGHNodePicker::s_instance;

void CGHNodePicker::Init()
{
	CGHRenderer::AddGlobalAction("nodePicker", 0, 3, std::bind(&CGHNodePicker::PickNode, this, std::placeholders::_1));
}

void CGHNodePicker::PickNode(CGHNode* node)
{
	if (node != nullptr)
	{
		if (node != m_currPickedNode)
		{
			if (m_currPickedNode)
			{
				m_currPickedNode->RemoveEvent(std::bind(&CGHNodePicker::TargetNodeDeleted, this), CGHNODE_EVENT_FLAG_DELETE);
			}

			m_currPickedNode = node;
			m_currPickedNode->AddEvent(std::bind(&CGHNodePicker::TargetNodeDeleted, this), CGHNODE_EVENT_FLAG_DELETE);
		}
	}
}

void CGHNodePicker::TargetNodeDeleted()
{
	m_currPickedNode = nullptr;
}
