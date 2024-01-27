#include <stack>
#include <string>
#include <vector>

#include <Windows.h>

#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>

#include "resource.h"

wil::com_ptr<ICoreWebView2Controller> webviewController;
wil::com_ptr<ICoreWebView2> webview;

static const wchar_t* kClassName = L"mini-browser";

std::wstring userAgentPC, userAgentMobile;

std::vector<std::wstring> SplitAgent(const std::wstring& userAgent)
{
	std::vector<std::wstring> v;

	auto userAgentIterEnd = userAgent.cend();
	for (auto iter = userAgent.cbegin(); iter != userAgentIterEnd; )
	{
		std::stack<wchar_t> s;
		std::wstring partial;
		while (iter != userAgentIterEnd)
		{
			if (*iter == L' ')
			{
				if (s.empty())
				{
					++iter;
					break;
				}
			}

			partial.push_back(*iter);

			if (*iter == L'(')
			{
				s.push(*iter);
			}
			else if (*iter == L')')
			{
				s.pop();
			}

			++iter;
		}

		v.push_back(std::move(partial));
	}

	return v;
}

std::wstring GetMobileAgent(const std::wstring& userAgentPC)
{
	auto partials = SplitAgent(userAgentPC);
	
	// Usually, the format of User-Agent header in Edge is the following:
	// Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0
	if (partials.size() < 7)
	{
		return {};
	}

	// Samsung Galaxy S20 5G
	partials[1] = L"(Linux; Android 13; SM-G981B)";
	partials.push_back(L"Mobile");

	std::wstring userAgentMobile;
	for (auto&& p : partials)
	{
		userAgentMobile += p;
		userAgentMobile.push_back(L' ');
	}

	userAgentMobile.pop_back();
	return userAgentMobile;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i = 0;
	switch (uMsg)
	{
	case WM_SIZE:
		if (webviewController)
		{
			webviewController->put_Bounds({ 0, 0, LOWORD(lParam), HIWORD(lParam) });
		}
		return 0;
	case WM_COMMAND:
		if (HIWORD(wParam) == 0)
		{
			MENUITEMINFO mii = { sizeof(MENUITEMINFO), };
			mii.fMask = MIIM_STATE;
			GetMenuItemInfo(GetMenu(hWnd), LOWORD(wParam), FALSE, &mii);
			switch (LOWORD(wParam))
			{
			case ID_40001: // 모바일모드
			{
				if (webview)
				{
					wil::com_ptr<ICoreWebView2Settings2> settings;
					webview->get_Settings((ICoreWebView2Settings**)&settings);

					if (mii.fState & MFS_CHECKED) // CHECKED -> UNCHECKED
					{
						mii.fState &= ~MFS_CHECKED;
						settings->put_UserAgent(userAgentPC.c_str());
					}
					else                          // UNCHEKCED -> CHECKED
					{
						mii.fState |= MFS_CHECKED;
						settings->put_UserAgent(userAgentMobile.c_str());
					}
					SetMenuItemInfo(GetMenu(hWnd), LOWORD(wParam), FALSE, &mii);
					webview->CallDevToolsProtocolMethod(L"Network.clearBrowserCache", L"{}",
						Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
							[](HRESULT errorCode, LPCWSTR returnObjectAsJson) -> HRESULT {
								webview->Reload();
								return S_OK;
							}).Get());
				}
			}
			break;
			case ID_40002: // 항상위에
			{
				if (mii.fState & MFS_CHECKED) // CHECKED -> UNCHECKED
				{
					mii.fState &= ~MFS_CHECKED;
					SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
				}
				else                          // UNCHEKCED -> CHECKED
				{
					mii.fState |= MFS_CHECKED;
					SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
				}
				SetMenuItemInfo(GetMenu(hWnd), LOWORD(wParam), FALSE, &mii);
			}
			break;
			}
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR pszCmdLine, _In_ int nCmdShow)
{
	WNDCLASSEX wc = { 0 };
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance     = hInstance;
	wc.lpfnWndProc   = &WndProc;
	wc.lpszClassName = kClassName;
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);
	wc.style         = CS_VREDRAW | CS_HREDRAW;
	RegisterClassEx(&wc);

	HWND hWnd = CreateWindow(
		kClassName,
		kClassName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		400,
		800,
		NULL,
		NULL,
		hInstance,
		nullptr);

	ShowWindow(hWnd, nCmdShow);

	CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
				env->CreateCoreWebView2Controller(hWnd, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
					[hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
						if (controller != nullptr) {
							webviewController = controller;
							webviewController->get_CoreWebView2(&webview);
						}

						wil::com_ptr<ICoreWebView2Settings2> settings;
						webview->get_Settings((ICoreWebView2Settings**)&settings);
						settings->put_IsScriptEnabled(TRUE);
						settings->put_AreDefaultScriptDialogsEnabled(TRUE);
						settings->put_IsWebMessageEnabled(TRUE);

						LPWSTR pszUserAgent = nullptr;
						settings->get_UserAgent(&pszUserAgent);
						userAgentPC = pszUserAgent;
						CoTaskMemFree(pszUserAgent);
						userAgentMobile = GetMobileAgent(userAgentPC);

						RECT bounds;
						GetClientRect(hWnd, &bounds);
						webviewController->put_Bounds(bounds);

						webview->Navigate(L"https://www.youtube.com/");

						return S_OK;
					}).Get());
				return S_OK;
			}).Get());

	MSG msg = { 0 };
	while (static_cast<int>(GetMessage(&msg, NULL, 0, 0)) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return static_cast<int>(msg.wParam);
}