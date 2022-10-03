#define WIN32_LEAN_AND_MEAN         
#include <windows.h>

#include "App.h"
#include <exception>
#include <string>
#include <assert.h>
#include "DxException.h"
#include "GraphicDeivceDX12.h"


LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

App* App::s_App = nullptr;

HRESULT App::Init()
{
	HRESULT result = InitWindow();
	GraphicDeviceDX12::CreateDeivce(m_hMainWnd, CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);

	return result;
}

int App::Run()
{
	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (m_appPaused)
			{
				int Test = 0;
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return static_cast<int>(msg.wParam);
}

App::App(HINSTANCE hInstance)
	: m_hAppInst(hInstance)
{
	assert(s_App == nullptr);
	s_App = this;
}

App::~App()
{
	GraphicDeviceDX12::DeleteDeivce();
}

App* App::GetApp()
{
	return s_App;
}

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SIZE:
	{
		int clientWidth = LOWORD(lParam);
		int clientHeight = HIWORD(lParam);

		CGH::GO.WIN.WindowsizeX = clientWidth;
		CGH::GO.WIN.WindowsizeY = clientHeight;

		if (wParam == SIZE_MINIMIZED)
		{
			m_miniMized = true;
			m_maximized = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			m_miniMized = false;
			m_maximized = true;
			GraphicDeviceDX12::GetGraphic()->OnResize(CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);
		}
		else if (wParam == SIZE_RESTORED)
		{
			if (m_miniMized)
			{
				m_miniMized = false;
				GraphicDeviceDX12::GetGraphic()->OnResize(CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);
			}
			else if (m_maximized)
			{
				m_maximized = false;
				GraphicDeviceDX12::GetGraphic()->OnResize(CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);
			}
			else if (m_resizing)
			{
				GraphicDeviceDX12::GetGraphic()->OnResize(CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);
			}
		}
		return 0;
	}
	case WM_ENTERSIZEMOVE:
	{
		m_resizing = true;
		return 0;
	}
	case WM_EXITSIZEMOVE:
	{
		GraphicDeviceDX12::GetGraphic()->OnResize(CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY);
		m_resizing = false;
		return 0;
	}
	case WM_GETMINMAXINFO:
	{
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);
	case WM_IME_STARTCOMPOSITION:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	{
		msg = 0;
	}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HRESULT App::InitWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return S_FALSE;
	}

	RECT R = { 0,0,CGH::GO.WIN.WindowsizeX, CGH::GO.WIN.WindowsizeY };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	m_hMainWnd = CreateWindow(L"MainWnd", L"Created",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_hAppInst, 0);
	if (!m_hMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return S_FALSE;
	}

	ShowWindow(m_hMainWnd, SW_SHOW);
	UpdateWindow(m_hMainWnd);
	return S_OK;
}

void App::Update(float delta)
{
}

int wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		App theApp(hInstance);

		if (theApp.Init() != S_OK)
		{
			return 0;
		}

		return theApp.Run();
	}
	catch (std::exception e)
	{
		std::string stringMessage(e.what());
		std::wstring exceptionMessage(stringMessage.begin(), stringMessage.end());
		MessageBox(nullptr, exceptionMessage.c_str(), L"HR Failed", MB_OK);
		return 0;
	}
	catch (DxException e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}

	return 0;
}