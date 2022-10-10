#pragma once
#include <assert.h>
#include <intsafe.h>
#include <DirectXMath.h>

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

	const DirectX::XMFLOAT4X4 IdentityMatrix(1.0f, 0.0f, 0.0f, 0.0f,
											 0.0f, 1.0f, 0.0f, 0.0f,
											 0.0f, 0.0f, 1.0f, 0.0f,
											 0.0f, 0.0f, 0.0f, 1.0f);
}

