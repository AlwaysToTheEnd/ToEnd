#include "Camera.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../Common/Source/CGHUtil.h"
#include "../Common/Source/d3dx12.h"
#include "DirectXTK/Keyboard.h"

using namespace DirectX;

Camera::Camera()
{
	m_viewMat = CGH::IdentityMatrix;
	m_eyePos = { 0,0,0 };
	m_offset = { 0,0,0 };
	m_dir = { 0,0,0 };
	m_angles = { 0,0 };
	m_currMouse = { 0,0 };
	m_prevMouse = { 0,0 };
	m_isRButtonDown = false;
	m_distance = 5;
	m_target = nullptr;
}

void Camera::Update()
{
	XMVECTOR eyePos = DirectX::XMVectorSet(0, 0, -m_distance, 1);
	XMVECTOR lookAt = DirectX::XMVectorZero();

	eyePos = XMVector3TransformCoord(eyePos, DirectX::XMMatrixRotationRollPitchYawFromVector(XMVectorSet(m_angles.y, m_angles.x, 0, 0)));
	XMStoreFloat3(&m_dir, DirectX::XMVector3Normalize(-eyePos));

	if (m_target)
	{
		lookAt = XMLoadFloat3(m_target);
		eyePos = lookAt + eyePos;
		
	}
	else
	{
		eyePos.m128_f32[0] += m_offset.x;
		eyePos.m128_f32[1] += m_offset.y;
		eyePos.m128_f32[2] += m_offset.z;

		lookAt = XMVectorSet(m_offset.x, m_offset.y, m_offset.z, 0);
	}

	XMStoreFloat3(&m_eyePos, eyePos);
	XMStoreFloat4x4(&m_viewMat, DirectX::XMMatrixLookAtLH(eyePos, lookAt, { 0,1,0,0 }));
}

void Camera::WndProc(int* hWND, unsigned int message, unsigned int* wParam, int* lParam)
{
	switch (message)
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
	}
}

DirectX::XMFLOAT3A Camera::GetViewRay(const DirectX::XMFLOAT4X4& projectionMat, unsigned int viewPortWidth, unsigned int viewPortHeight) const
{
	return XMFLOAT3A((2.0f * m_currMouse.x / viewPortWidth - 1.0f) / projectionMat.m[0][0],
		(-2.0f * m_currMouse.y / viewPortHeight + 1.0f) / projectionMat.m[1][1], 1.0f);
}
