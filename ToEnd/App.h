#pragma once
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN         
#include <windows.h>
#include <string>

#include "../Common/Source/GameTimer.h"

namespace CGH
{
	static struct GlobalOptions
	{
		struct WindowOption
		{
			int WindowsizeX = 900;
			int WindowsizeY = 900;
		}WIN;

		struct GraphicOption
		{
			const std::wstring	TextureFolderPath = L"./../Common/Texture";
			const std::wstring	FontFolderPath = L"./../Common/Fonts";
			const std::wstring	SpriteTextureSuffix = L"sp_";
		}GRAPHIC;
	}GO;
}

class App final
{
public:
	App(HINSTANCE hInstance);
	App(const App& rhs) = delete;
	App& operator=(const App& rhs) = delete;

	virtual ~App();

	static App*	GetApp();

	HRESULT	Init();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	int		Run();

private:
	HRESULT InitWindow();
	void	Update(float delta);
	void	Render();
	void	CalculateFrame();

private:
	static App*	 s_App;

	HINSTANCE	m_hAppInst = nullptr;
	HWND		m_hMainWnd = nullptr;
	bool		m_appPaused = false;
	bool		m_resizing = false;
	bool		m_miniMized = false;
	bool		m_maximized = false;

	GameTimer	m_timer;
};