#pragma once
#include <assert.h>
#include <intsafe.h>
#include <string>
#include <DirectXMath.h>

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
		const std::wstring	TextureFolderPath = L"Texture";
		const std::wstring	FontFolderPath = L"Fonts";
		float				Shadowlength = 100.0f;
		float				phongTessAlpha = 0.75f;
	}GRAPHIC;

	static GlobalOptions GO;
};
