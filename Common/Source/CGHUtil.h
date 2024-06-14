#pragma once
#include <cmath>
#include <assert.h>
#include <intsafe.h>
#include <string>
#include <DirectXMath.h>
#include <DirectXColors.h>

#define GET_NAME(n) #n

namespace CGH
{
	inline unsigned int SizeTTransUINT(size_t size)
	{
		assert(UINT_MAX > size);
		return static_cast<unsigned int>(size);
	}

	inline int SizeTTransINT(size_t size)
	{
		assert(INT_MAX > size);
		return static_cast<int>(size);
	}

	static const DirectX::XMFLOAT4X4 IdentityMatrix(1.0f, 0.0f, 0.0f, 0.0f,
											 0.0f, 1.0f, 0.0f, 0.0f,
											 0.0f, 0.0f, 1.0f, 0.0f,
											 0.0f, 0.0f, 0.0f, 1.0f);
	const unsigned int NumFrameresource = 3;

	inline void QuaternionToAngularAngles(const DirectX::XMVECTOR& q, float& roll, float& pitch, float& yaw)
	{
		float qx = DirectX::XMVectorGetX(q);
		float qy = DirectX::XMVectorGetY(q);
		float qz = DirectX::XMVectorGetZ(q);
		float qw = DirectX::XMVectorGetW(q);

		float siny_cosp = 2.0f * (qw * qz + qx * qy);
		float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
		yaw = std::atan2(siny_cosp, cosy_cosp);

		float sinp = 2.0f * (qw * qy - qz * qx);
		if (std::abs(sinp) >= 1.0f)
			pitch = std::copysign(DirectX::XM_PI / 2.0f, sinp);
		else
			pitch = std::asin(sinp);

		float sinr_cosp = 2.0f * (qw * qx + qy * qz);
		float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
		roll = std::atan2(sinr_cosp, cosr_cosp);
	}
}

struct GlobalOptions
{
	struct WindowOption
	{
		int WindowsizeX = 900;
		int WindowsizeY = 900;
	}WIN;

	struct GraphicOption
	{
		DirectX::XMVECTORF32	BGColor = { { { 0.f, 0.f, 0.f, 0.f } } };
		bool 					EnableWireFrame = false;
		bool					EnableTextureDebugPSO = false;

		float					PhongTessAlpha = 0.75f;
		float					PhongTessFactor = 4.0f;

		bool					EnableSMAA = true;	
		float 					SMAAEdgeDetectionThreshold = 0.1f;
		int						SMAAMaxSearchSteps = 16;
		int						SMAAMaxSearchStepsDiag = 8;
		int						SMAACornerRounding = 25;

	}GRAPHIC;

	static GlobalOptions GO;
};
