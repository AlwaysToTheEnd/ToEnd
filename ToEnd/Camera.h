#pragma once
#include <DirectXMath.h>

class Camera
{
public:

public:
	Camera();
	~Camera() = default;

	void Update();

	DirectX::XMFLOAT3A			GetViewRay(const DirectX::XMFLOAT4X4& projectionMat, unsigned int viewPortWidth, unsigned int viewPortHeight) const;
	const DirectX::XMFLOAT4X4*	GetViewMatrix() const { return &m_viewMat; }
	const DirectX::XMFLOAT3&	GetEyePos() const { return m_eyePos; }
	const DirectX::XMFLOAT3&	GetTargetPos() const { return m_targetPos; }
	float						GetDistance() const { return m_distance; }

private:
	DirectX::XMFLOAT4X4 m_viewMat;
	DirectX::XMFLOAT3	m_eyePos;
	DirectX::XMFLOAT3	m_targetPos;
	DirectX::XMFLOAT3	m_angles;
	float				m_distance;
	DirectX::XMINT2		m_currMouse;
	DirectX::XMINT2		m_prevMouse;
};

