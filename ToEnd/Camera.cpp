#include "Camera.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/d3dx12.h"
#include "InputManager.h"
#include "CGHBaseClass.h"

using namespace DirectX;

Camera::Camera()
{
	m_viewMat = CGH::IdentityMatrix;
	m_eyePos = { 0,0,-10 };
	m_currMouse = { 0,0 };
	m_prevMouse = { 0,0 };
	m_targetPos = {};
	m_rotateQuater = {};
	m_distance = 10;
}

void Camera::Update()
{

	auto& mouse = InputManager::GetMouse();
	auto mouseState = mouse.GetLastState();
	m_prevMouse = m_currMouse;
	m_currMouse = { mouseState.x, mouseState.y };

	if (mouse.middleButton == 1)
	{
		m_eyePos.x -= (m_currMouse.x - m_prevMouse.x) * 0.01f;
		m_eyePos.y += (m_currMouse.y - m_prevMouse.y) * 0.01f;
	}

	XMVECTOR eyePos = DirectX::XMLoadFloat3(&m_eyePos);
	XMVECTOR target = eyePos;
	target.m128_f32[2] = m_eyePos.z + m_distance;

	if (mouse.rightButton == 1)
	{
	}

	if (mouse.rightButton == 3)
	{
		m_targetMouse = m_currMouse;
	}

	m_distance -= mouseState.scrollWheelValue * 0.00002f;

	

	XMStoreFloat4x4(&m_viewMat, DirectX::XMMatrixLookAtLH(eyePos, target, { 0,1,0,0 }));
}

void Camera::WndProc(int* hWND, unsigned int message, unsigned int* wParam, int* lParam)
{
	/*switch (message)
	{
	case WM_RBUTTONDOWN	:
	{
		m_prevMouse.x = LOWORD(lParam);
		m_prevMouse.y = HIWORD(lParam);
		m_isRButtonDown = true;
	}
	break;
	case WM_RBUTTONUP:
	{
		m_isRButtonDown = false;
	}
	break;
	case WM_MOUSEMOVE:
	{
		m_currMouse.x = LOWORD(lParam);
		m_currMouse.y = HIWORD(lParam);

		if (m_isRButtonDown)
		{
			XMFLOAT2 movement(m_currMouse.x - m_prevMouse.x, m_currMouse.y - m_prevMouse.y);
			m_angles.x += movement.x / 100.0f;
			m_angles.y += movement.y / 100.0f;

			if (m_angles.y <= -g_XMPi.f[0] * 0.5f + FLT_EPSILON)
			{
				m_angles.y = -g_XMPi.f[0] * 0.5f + FLT_EPSILON;
			}
			else if (m_angles.y >= g_XMPi.f[0] * 0.5f - FLT_EPSILON)
			{
				m_angles.y = g_XMPi.f[0] * 0.5f - FLT_EPSILON;
			}

			m_prevMouse = m_currMouse;
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		m_distance -= GET_WHEEL_DELTA_WPARAM(wParam) / 100.0f;
	}
	break;
	}*/
}

DirectX::XMFLOAT3A Camera::GetViewRay(const DirectX::XMFLOAT4X4& projectionMat, unsigned int viewPortWidth, unsigned int viewPortHeight) const
{
	return XMFLOAT3A((2.0f * m_currMouse.x / viewPortWidth - 1.0f) / projectionMat.m[0][0],
		(-2.0f * m_currMouse.y / viewPortHeight + 1.0f) / projectionMat.m[1][1], 1.0f);
}
