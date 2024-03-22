#define WIN32_LEAN_AND_MEAN         
#include <windows.h>

#include "App.h"
#include <exception>
#include <string>
#include <assert.h>
#include "CGHBaseClass.h"
#include "../Common/Source/DxException.h"
#include "GraphicDeivceDX12.h"
#include "DX12GarbageFrameResourceMG.h"
#include "DX12DefaultBufferCreator.h"
#include "DX12GraphicResourceManager.h"
#include "DX12TextureBuffer.h"

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

App* App::s_App = nullptr;

HRESULT App::Init()
{
	HRESULT result = InitWindow();
	GraphicDeviceDX12::CreateDeivce(m_hMainWnd, GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
	m_timer.Start();
	DX12GraphicResourceManager::s_insatance.Init();
	DX12GarbageFrameResourceMG::s_instance.Init();
	DX12DefaultBufferCreator::instance.Init();
	DX12TextureBuffer::Init();

	m_testScene = new TestScene();
	m_testScene->Init();

	return result;
}

int App::Run()
{
	MSG msg = {};
	m_timer.Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_timer.Tick();

			if (!m_appPaused)
			{
				CalculateFrame();
				Update(m_timer.DeltaTime());

				Render();
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
	delete m_testScene;
	GraphicDeviceDX12::DeleteDeivce();
}

App* App::GetApp()
{
	return s_App;
}

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	m_camera.WndProc(reinterpret_cast<int*>(hwnd), msg, reinterpret_cast<unsigned int*>(wParam), reinterpret_cast<int*>(lParam));

	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			m_appPaused = true;
			m_timer.Stop();
		}
		else
		{
			m_appPaused = false;
			m_timer.Start();
		}
		return 0;
	case WM_SIZE:
	{
		int clientWidth = LOWORD(lParam);
		int clientHeight = HIWORD(lParam);

		GO.WIN.WindowsizeX = clientWidth;
		GO.WIN.WindowsizeY = clientHeight;

		if (wParam == SIZE_MINIMIZED)
		{
			m_miniMized = true;
			m_maximized = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			m_miniMized = false;
			m_maximized = true;
			GraphicDeviceDX12::GetGraphic()->OnResize(GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
		}
		else if (wParam == SIZE_RESTORED)
		{
			if (m_miniMized)
			{
				m_miniMized = false;
				GraphicDeviceDX12::GetGraphic()->OnResize(GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
			}
			else if (m_maximized)
			{
				m_maximized = false;
				GraphicDeviceDX12::GetGraphic()->OnResize(GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
			}
			else if (m_resizing)
			{
				GraphicDeviceDX12::GetGraphic()->OnResize(GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
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
		GraphicDeviceDX12::GetGraphic()->OnResize(GO.WIN.WindowsizeX, GO.WIN.WindowsizeY);
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

	RECT R = { 0,0,GO.WIN.WindowsizeX, GO.WIN.WindowsizeY };
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
	m_camera.Update();
	GraphicDeviceDX12::GetGraphic()->Update(delta, &m_camera);
	DX12GarbageFrameResourceMG::s_instance.TryClearJunks();

	m_testScene->Update(delta);

	CGHNode::ClearNodeEvents();
}

void App::Render()
{
	auto graphic = GraphicDeviceDX12::GetGraphic();
	graphic->RenderBegin();

	m_testScene->Render();

	graphic->RenderEnd();
}

void App::CalculateFrame()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = std::wstring(L"누가죽나 해보자") +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(m_hMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
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