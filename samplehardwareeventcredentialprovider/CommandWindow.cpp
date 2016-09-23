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
#include <atlbase.h>
// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

static IEnumWbemClassObject* pEnumerator;
static IWbemLocator *pLoc;
static IWbemServices *pSvc;
static IWbemClassObject *pclsObj;

const WCHAR c_szClassName[] = L"EventWindow";
const WCHAR c_szConnected[] = L"Connected";
const WCHAR c_szDisconnected[] = L"Disconnected";


// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

CCommandWindow::CCommandWindow() : _hWnd(NULL), _hInst(NULL), _fConnected(FALSE), _pProvider(NULL)
{
}

CCommandWindow::~CCommandWindow()
{
	// If we have an active window, we want to post it an exit message.
	if (_hWnd != NULL)
	{
		PostMessage(_hWnd, WM_EXIT_THREAD, 0, 0);
		_hWnd = NULL;
	}

	// We'll also make sure to release any reference we have to the provider.
	if (_pProvider != NULL)
	{
		_pProvider->Release();
		_pProvider = NULL;
	}

	pSvc->Release();
	pLoc->Release();
	pEnumerator->Release();
	pclsObj->Release();
}

// Performs the work required to spin off our message so we can listen for events.
HRESULT CCommandWindow::Initialize(__in CSampleProvider *pProvider)
{
	HRESULT hr = S_OK;

	// Be sure to add a release any existing provider we might have, then add a reference
	// to the provider we're working with now.
	if (_pProvider != NULL)
	{
		_pProvider->Release();
	}
	_pProvider = pProvider;
	_pProvider->AddRef();

	// Create and launch the window thread.
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


HRESULT CCommandWindow::_InitInstance()
{
	HRESULT hr = S_OK;

	// Create our window to receive events.
	// 
	// This dialog is for demonstration purposes only.  It is not recommended to create 
	// dialogs that are visible even before a credential enumerated by this credential 
	// provider is selected.  Additionally, any dialogs that are created by a credential
	// provider should not have a NULL hwndParent, but should be parented to the HWND
	// returned by ICredentialProviderCredentialEvents::OnCreatingWindow.
	_hWnd = CreateWindowEx(
		WS_EX_TOPMOST,
		c_szClassName,
		c_szDisconnected,
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
		// Add a button to the window.
		_hWndButton = CreateWindow(L"Button", L"Press to connect",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			10, 10, 180, 30,
			_hWnd,
			NULL,
			_hInst,
			NULL);
		if (_hWndButton == NULL)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}

		if (SUCCEEDED(hr))
		{
			// Show and update the window.
			if (!ShowWindow(_hWnd, SW_NORMAL))
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
	}

	return hr;
}

// Called from the separate thread to process the next message in the message queue. If
// there are no messages, it'll wait for one.
BOOL CCommandWindow::_ProcessNextMessage()
{
	// Grab, translate, and process the message.
	MSG msg;
	GetMessage(&(msg), _hWnd, 0, 0);
	TranslateMessage(&(msg));
	DispatchMessage(&(msg));

	// This section performs some "post-processing" of the message. It's easier to do these
	// things here because we have the handles to the window, its button, and the provider
	// handy.
	switch (msg.message)
	{
		// Return to the thread loop and let it know to exit.
	case WM_EXIT_THREAD: return FALSE;

		// Toggle the connection status, which also involves updating the UI.
		// получаем пост-сообщение о подключении
	case WM_TOGGLE_CONNECTED_STATUS:
		// здесь ловится подключение к провайдеру и, соответственно, зум тайла.
		_fConnected = !_fConnected;
		/*if (_fConnected)
		{
			SetWindowText(_hWnd, c_szConnected);
			SetWindowText(_hWndButton, L"Press to disconnect");
		}
		else
		{
			SetWindowText(_hWnd, c_szDisconnected);
			SetWindowText(_hWndButton, L"Press to connect");
		}*/
		Initialize(_pProvider);
		_pProvider->OnConnectStatusChanged();
		break;
	}
	return TRUE;
}



std::string ConvertWCSToMBS(const wchar_t* pstr, long wslen)
{
	int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

	std::string dblstr(len, '\0');
	len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */,
		pstr, wslen /* not necessary NULL-terminated */,
		&dblstr[0], len,
		NULL, NULL /* no default char */);

	return dblstr;
}
BSTR ConvertMBSToBSTR(const std::string& str)
{
	int wslen = ::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
		str.data(), str.length(),
		NULL, 0);

	BSTR wsdata = ::SysAllocStringLen(NULL, wslen);
	::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
		str.data(), str.length(),
		wsdata, wslen);
	return wsdata;
}
std::string ConvertBSTRToMBS(BSTR bstr)
{
	int wslen = ::SysStringLen(bstr);
	return ConvertWCSToMBS((wchar_t*)bstr, wslen);
}


// Manages window messages on the window thread.
LRESULT CALLBACK CCommandWindow::_WndProc(__in HWND hWnd, __in UINT message, __in WPARAM wParam, __in LPARAM lParam)
{

	HDEVNOTIFY hDevNotify;
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	for (int i = 0; i<sizeof(GUID_DEVINTERFACE_LIST) / sizeof(GUID); i++) {
	NotificationFilter.dbcc_classguid = { 0x50DD5230, 0xBA8A, 0x11D1,{ 0xB5, 0xFD, 0x00, 0x00, 0xF8, 0x05, 0xF5, 0x30 } };
	hDevNotify = RegisterDeviceNotification(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (!hDevNotify) {
			//MessageBox(NULL, L"Usb-device with this GUID NOT registered", L"Debug", 0);
			return FALSE;
		}
		else 
		{
			string query = "SELECT * FROM Win32_PnPEntity WHERE DeviceID='USB\\\\VID_0A89&PID_0030\\\\7&25C389C1&0&1'";
			bstr_t _query = ConvertMBSToBSTR(query);
			//MessageBox(NULL, L"Usb-device with this GUID REGISTERED", L"Debug", 0);
			//MessageBox(NULL, L"WM_DEVICECHANGE", L"Handled:", 0);
			MessageBox(NULL, _query, L"query is", 0); //DEVICEID is correct
													  //MessageBox(NULL, DEVICEID, L"Consts.h is", 0); //DEVICEID is correct
			HRESULT hres = pSvc->ExecQuery(
				// WQL-запрос интерфейса
				bstr_t("WQL"),
				bstr_t(_query),
				WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
				NULL,
				&pEnumerator);
			if (FAILED(hres))
			{
				pSvc->Release();
				pLoc->Release();
				CoUninitialize();
				MessageBox(NULL, L"Device Handle Failed while requesting WQL", L"Debug", 0);
				return 1;               // Program has failed
			}

			ULONG uReturn = 0;

			while (pEnumerator)
			{
				HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
					&pclsObj, &uReturn);

				if (0 == uReturn)
					break;

				VARIANT vtProp;
				hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
				MessageBox(NULL, vtProp.bstrVal, L"Debug", 0);

				if (wcscmp(vtProp.bstrVal, DEVICEID) == 0) //strings comparison: EQUAL
				{
					hr = HRESULT_FROM_WIN32(GetLastError());
					MessageBox(NULL, vtProp.bstrVal, L"vtProp.bstrval is:", 0);
					MessageBox(NULL, L"DeviceID equal", L"Debug", 0);
					
					message = WM_TOGGLE_CONNECTED_STATUS;
				}
				else
				{
					//MessageBox(NULL, vtProp.bstrVal, L"PNPDeviceID is:", 0);
					MessageBox(NULL, L"PNPDeviceIDs not Equal ", L"Debug", 0);

				}
				VariantClear(&vtProp);

				pclsObj->Release();
			}
			//MessageBox(NULL, L"WM_DEVICECHANGE Handled Successfully", L"Debug", 0);
		}
	}

	//SELECT * FROM Win32_PnPEntity WHERE DeviceID='USB\\VID_0A89&PID_0030\\7&25C389C1&0&1'
	
	//MessageBox(NULL, _query, L"Query is:", 0);

	switch (message)
	{
		// Originally we were going to work with USB keys being inserted and removed, but it
		// seems as though these events don't get to us on the secure desktop. However, you
		// might see some messageboxi in CredUI.
		// TODO: Remove if we can't use from LogonUI.
	case WM_DEVICECHANGE:
	{
		//PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0); //WM_TOGGLE_CONNECTED_STATUS
	}
		break;

		// После нажатия на кнопку сообщение преобразуется в WM_TOGGLE_CONNECTED_STATUS.
	case WM_COMMAND:
		//PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0);
		break;

		// To play it safe, we hide the window when "closed" and post a message telling the 
		// thread to exit.
	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		PostMessage(hWnd, WM_EXIT_THREAD, 0, 0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Our thread procedure. We actually do a lot of work here that could be put back on the 
// main thread, such as setting up the window, etc.



DWORD WINAPI CCommandWindow::_ThreadProc(__in LPVOID lpParameter)
{
	CCommandWindow *pCommandWindow = static_cast<CCommandWindow *>(lpParameter);
	if (pCommandWindow == NULL)
	{
		// TODO: What's the best way to raise this error?
		return 0;
	}

	HRESULT hr = S_OK;


	HRESULT hres;
	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres))
	MessageBox(NULL, L"FAILED(hres)", L"Debug", 0);                // Program has failed.

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
	MessageBox(NULL, L"Failed to Connect to ROOR\CIMV2", L"Debug", 0);
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
	//***************************************************************
	

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

	// ProcessNextMessage will pump our message pump and return false if it comes across
	// a message telling us to exit the thread.
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

