#pragma once
#include <DirectXMath.h>

class Camera
{
public:
	enum class CAMERA_MOVE_DIR
	{
		DIR_FRONT,
		DIR_BACK,
		DIR_RIGHT,
		DIR_LEFT,
		DIR_ORIGIN
	};

public:
	Camera();
	~Camera() = default;

	void Update();
	void WndProc(int* hWND, unsigned int message, unsigned int* wParam, int* lParam);

	void SetDistance(float distance) { m_distance = distance; }

	DirectX::XMFLOAT3A			GetViewRay(const DirectX::XMFLOAT4X4& projectionMat, unsigned int viewPortWidth, unsigned int viewPortHeight) const;
	const DirectX::XMFLOAT4X4*	GetViewMatrix() const { return &m_viewMat; }
	DirectX::XMFLOAT3			GetEyePos() const { return m_eyePos; }
	DirectX::XMFLOAT2			GetMousePos() const { return m_currMouse; }

private:
	DirectX::XMFLOAT4X4 m_viewMat;
	DirectX::XMFLOAT3	m_eyePos;
	DirectX::XMFLOAT3	m_offset;
	DirectX::XMFLOAT3	m_dir;
	DirectX::XMFLOAT2	m_angles;
	DirectX::XMFLOAT2	m_currMouse;
	DirectX::XMFLOAT2	m_prevMouse;
	bool				m_isRButtonDown;
	float				m_distance;

	const DirectX::XMFLOAT3* m_target;
};

