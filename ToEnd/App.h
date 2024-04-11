#pragma once
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN         
#include <windows.h>
#include <string>
#include <memory>
#include "Camera.h"

#include "TestScene.h"

#include "../Common/Source/GameTimer.h"


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

	TestScene*	m_testScene;

	HINSTANCE	m_hAppInst = nullptr;
	HWND		m_hMainWnd = nullptr;
	bool		m_appPaused = false;
	bool		m_resizing = false;
	bool		m_miniMized = false;
	bool		m_maximized = false;

	Camera		m_camera;
	GameTimer	m_timer;
};