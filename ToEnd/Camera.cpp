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
	m_eyePos = { 0,0,-5 };
	m_currMouse = { 0,0 };
	m_prevMouse = { 0,0 };
	m_angles = { 0,0,0 };
	m_targetPos = { 0,0,0 };
	m_distance = 5.0f;
}

void Camera::Update()
{
	auto& mouse = InputManager::GetMouse();
	auto mouseState = mouse.GetLastState();
	m_prevMouse = m_currMouse;
	m_currMouse = { mouseState.x, mouseState.y };
	float mouseMoveX = (m_currMouse.x - m_prevMouse.x);
	float mouseMoveY = (m_currMouse.y - m_prevMouse.y);

	XMVECTOR eyePos = XMLoadFloat3(&m_eyePos);
	XMVECTOR targetPos = XMLoadFloat3(&m_targetPos);
	XMVECTOR upVector = XMVectorSet(0, 1, 0, 0);
	XMVECTOR toEye = eyePos - targetPos;

	float distance = XMVectorGetX(XMVector3Length(toEye));
	XMVECTOR normalDir = XMVector3Normalize(toEye);

	distance -= mouseState.scrollWheelValue * 0.01f;
	eyePos = targetPos + normalDir * distance;

	if (mouse.rightButton == 1)
	{
		if (mouseMoveX != 0 || mouseMoveY != 0)
		{
			m_angles.x += mouseMoveY * 0.01f;
			m_angles.y += mouseMoveX * 0.01f;
			XMVECTOR rotQuat = XMQuaternionRotationRollPitchYaw(m_angles.x, m_angles.y, m_angles.z);
			toEye = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), rotQuat);

			if (abs(m_angles.x) >= XM_2PI)
			{
				if (m_angles.x > 0.0f)
				{
					m_angles.x -= XM_2PI;
				}
				else
				{
					m_angles.x += XM_2PI;
				}
			}
			
			if (abs(m_angles.y) >= XM_2PI)
			{
				if (m_angles.y > 0.0f)
				{
					m_angles.y -= XM_2PI;
				}
				else
				{
					m_angles.y += XM_2PI;
				}
			}

			eyePos = targetPos + toEye * distance;
		}
	}

	if (m_angles.x >= 0.0f)
	{
		if (m_angles.x >= XM_PIDIV2 && m_angles.x < XM_PI + XM_PIDIV2)
		{
			upVector = XMVectorSet(0, -1, 0, 0);
		}
	}
	else
	{
		if (abs(m_angles.x) >= XM_PIDIV2 && abs(m_angles.x) < XM_PI + XM_PIDIV2)
		{
			upVector = XMVectorSet(0, -1, 0, 0);
		}
	}

	if (mouse.middleButton == 1)
	{
		XMVECTOR rightDir = XMVector3Cross(upVector, -normalDir);
		XMVECTOR upDir = XMVector3Cross(-normalDir, rightDir);

		eyePos -= (rightDir * mouseMoveX * 0.01f);
		eyePos += (upDir * mouseMoveY * 0.01f);

		targetPos -= (rightDir * mouseMoveX * 0.01f);
		targetPos += (upDir * mouseMoveY * 0.01f);
	}

	m_distance = distance;
	XMStoreFloat3(&m_eyePos, eyePos);
	XMStoreFloat3(&m_targetPos, targetPos);
	XMStoreFloat4x4(&m_viewMat, XMMatrixLookAtLH(eyePos, targetPos, upVector));
}

DirectX::XMFLOAT3A Camera::GetViewRay(const DirectX::XMFLOAT4X4& projectionMat, unsigned int viewPortWidth, unsigned int viewPortHeight) const
{
	return XMFLOAT3A((2.0f * m_currMouse.x / viewPortWidth - 1.0f) / projectionMat.m[0][0],
		(-2.0f * m_currMouse.y / viewPortHeight + 1.0f) / projectionMat.m[1][1], 1.0f);
}
