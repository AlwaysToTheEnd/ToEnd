#include "CGHNodePicker.h"
#include "BaseComponents.h"

CGHNodePicker CGHNodePicker::s_instance;

void CGHNodePicker::Init()
{
	CGHRenderer::AddGlobalAction("nodePicker", 0, 3, std::bind(&CGHNodePicker::PickNode, this, std::placeholders::_1));
}

void CGHNodePicker::PickNode(CGHNode* node)
{
	/*if (node != nullptr)
	{
		auto layer = node->GetLayer();
		unsigned int layerIndex = static_cast<unsigned int>(layer);

		if (node != m_currPickedNode[layerIndex])
		{
			if (m_currPickedNode)
			{
				m_currPickedNode[layerIndex]->RemoveEvent(std::bind(&CGHNodePicker::TargetNodeDeleted, this, layer), CGHNODE_EVENT_FLAG_DELETE);
			}

			m_currPickedNode[layerIndex] = node;
			m_currPickedNode[layerIndex]->AddEvent(std::bind(&CGHNodePicker::TargetNodeDeleted, this, layer), CGHNODE_EVENT_FLAG_DELETE);
		}
	}*/
}

void CGHNodePicker::TargetNodeDeleted(CGHNODE_LAYER layer)
{
	m_currPickedNode[static_cast<unsigned int>(layer)] = nullptr;
}
