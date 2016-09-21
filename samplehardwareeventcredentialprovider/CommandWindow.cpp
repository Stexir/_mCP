//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//


#define _WIN32_DCOM
#include <iostream>
using namespace std;
#include <comdef.h>
#include <Wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")
# pragma comment(lib, "credui.lib")
# pragma comment(lib, "comsuppw.lib")
#include <wincred.h>
#include <strsafe.h>

#include "CommandWindow.h"
#include <strsafe.h>
#include <usbiodef.h>
#include "consts.h"
#include <Dbt.h>
// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

static IEnumWbemClassObject* pEnumerator;
static IWbemLocator *pLoc;
static IWbemServices *pSvc;
static IWbemClassObject *pclsObj;

const WCHAR c_szClassName[] = L"cheaterWindow";

CCommandWindow::CCommandWindow() : _hWnd(NULL), _hInst(NULL), _fConnected(FALSE), _pProvider(NULL)
{
	pEnumerator = NULL;
	pLoc = NULL;
	pSvc = NULL;
}

CCommandWindow::~CCommandWindow()
{
	if (_hWnd != NULL)
	{
		PostMessage(_hWnd, WM_EXIT_THREAD, 0, 0);
		_hWnd = NULL;
	}

	if (_pProvider != NULL)
	{
		_pProvider->Release();
		_pProvider = NULL;
	}

	pSvc->Release();
	pLoc->Release();
	pEnumerator->Release();
	pclsObj->Release();
	CoUninitialize();
}

HRESULT CCommandWindow::Initialize(__in CSampleProvider *pProvider)
{
	HRESULT hr = S_OK;

	if (_pProvider != NULL)
	{
		_pProvider->Release();
	}
	_pProvider = pProvider;
	_pProvider->AddRef();

	HANDLE hThread = CreateThread(NULL, 0, _ThreadProc, this, 0, NULL);
	if (hThread == NULL)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}

	return hr;
}

// Wraps our internal connected status so callers can easily access it.
BOOL CCommandWindow::GetConnectedStatus()
{
	return _fConnected;
}

//
//  FUNCTION: _MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
HRESULT CCommandWindow::_MyRegisterClass()
{
	WNDCLASSEX wcex = { sizeof(wcex) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = _WndProc;
	wcex.hInstance = _hInst;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = c_szClassName;

	return RegisterClassEx(&wcex) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

//
//   FUNCTION: _InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HRESULT CCommandWindow::_InitInstance()
{
	HRESULT hr = S_OK;

	_hWnd = CreateWindowEx(
		WS_EX_TOPMOST,
		c_szClassName,
		L"",
		WS_DLGFRAME,
		200, 200, 200, 80,
		NULL,
		NULL, _hInst, NULL);
	if (_hWnd == NULL)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}

	if (SUCCEEDED(hr))
	{
		if (!ShowWindow(_hWnd, SW_HIDE))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}

		if (SUCCEEDED(hr))
		{
			if (!UpdateWindow(_hWnd))
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
			}
		}
	}

	return hr;
}

BOOL CCommandWindow::_ProcessNextMessage()
{
	MSG msg;
	GetMessage(&(msg), _hWnd, 0, 0);
	TranslateMessage(&(msg));
	DispatchMessage(&(msg));

	switch (msg.message)
	{
	case WM_EXIT_THREAD: return FALSE;
	case WM_TOGGLE_CONNECTED_STATUS:
		_fConnected = true;
		_pProvider->OnConnectStatusChanged();
		break;
	}
	return TRUE;

}

LRESULT CALLBACK CCommandWindow::_WndProc(__in HWND hWnd, __in UINT message, __in WPARAM wParam, __in LPARAM lParam)
{
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	// assume we want to be notified with USBSTOR
	// to get notified with all interface on XP or above
	// ORed 3rd param with DEVICE_NOTIFY_ALL_INTERFACE_CLASSES and dbcc_classguid will be ignored
	//NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USBSTOR;
	HDEVNOTIFY hDevNotify = RegisterDeviceNotification(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (!hDevNotify) {
		// error handling...
		//return FALSE;
	}
	else
	{
		MessageBox(NULL, L"Device Connected successfully", L"Device Connected successfully", 0);
		message = WM_DEVICECHANGE;
	}
	switch (message)
	{
	case WM_DEVICECHANGE:
	{
		MessageBox(NULL, L"WM_DEVICECHANGE Handled Successfully", L"WM_DEVICECHANGE Handled Successfully", 0);
		HRESULT hres = pSvc->ExecQuery(
			// WQL-запрос интерфейса
			bstr_t("WQL"),
			bstr_t("SELECT * FROM Win32_DiskDrive"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			NULL,
			&pEnumerator);
		if (FAILED(hres))
		{
			pSvc->Release();
			pLoc->Release();
			CoUninitialize();
			MessageBox(NULL, L"Device Handle Failed while requesting WQL", L"Device Handle Failed while requesting WQL", 0);
			return 1;               // Program has failed
		}

		ULONG uReturn = 0;

		while (pEnumerator)
		{
			HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
				&pclsObj, &uReturn);

			if (0 == uReturn)
			{
				break;
			}

			VARIANT vtProp;

			hr = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
			
			if (wcscmp(vtProp.bstrVal, PNPID) == 0) //strings comparison: EQUAL
			{
				//MessageBox(NULL, L"PNPDeviceID", L"PNPDeviceID", 0);
				PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0);
			}
			VariantClear(&vtProp);

			pclsObj->Release();
		}
	}
	break;

	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		PostMessage(hWnd, WM_EXIT_THREAD, 0, 0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

DWORD WINAPI CCommandWindow::_ThreadProc(__in LPVOID lpParameter)
{
	CCommandWindow *pCommandWindow = static_cast<CCommandWindow *>(lpParameter);
	if (pCommandWindow == NULL)
	{
		return 0;
	}

	HRESULT hres;
	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres))
		return 1;                  // Program has failed.

	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *) &(pLoc));

	if (FAILED(hres))
	{
		CoUninitialize();
		return 1;                 // Program has failed.
	}
	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		NULL,                    // User name. NULL = current user
		NULL,                    // User password. NULL = current
		0,                       // Locale. NULL indicates current
		NULL,                    // Security flags.
		0,                       // Authority (e.g. Kerberos)
		0,                       // Context object 
		&pSvc                    // pointer to IWbemServices proxy
	);

	if (FAILED(hres))
	{
		pLoc->Release();
		CoUninitialize();
		return 1;                // Program has failed.
	}

	hres = CoSetProxyBlanket(
		pSvc,                        // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
	);

	if (FAILED(hres))
	{
		pSvc->Release();
		pLoc->Release();
		CoUninitialize();
		return 1;               // Program has failed.
	}



	HRESULT hr = S_OK;

	// Create the window.
	pCommandWindow->_hInst = GetModuleHandle(NULL);
	if (pCommandWindow->_hInst != NULL)
	{
		hr = pCommandWindow->_MyRegisterClass();
		if (SUCCEEDED(hr))
		{
			hr = pCommandWindow->_InitInstance();
		}
	}
	else
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	ShowWindow(pCommandWindow->_hWnd, SW_HIDE);

	if (SUCCEEDED(hr))
	{
		while (pCommandWindow->_ProcessNextMessage())
		{
		}
	}
	else
	{
		if (pCommandWindow->_hWnd != NULL)
		{
			pCommandWindow->_hWnd = NULL;
		}
	}

	return 0;
}

