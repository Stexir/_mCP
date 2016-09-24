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





// Our callback class must implement IPortableDeviceEventCallback
class CPortableDeviceEventsCallback : public IPortableDeviceEventCallback
{
public:
	CPortableDeviceEventsCallback() :
		m_cRef(1), m_pwszEventCookie(NULL)
	{
	}

	~CPortableDeviceEventsCallback()
	{
	}

	// Standard QI implementation for IID_IPortableDeviceEventCallback
	HRESULT __stdcall QueryInterface(
		REFIID  riid,
		LPVOID* ppvObj);

	// Standard AddRef implementation
	ULONG __stdcall AddRef();

	// Standard Release implementation
	ULONG __stdcall Release();

	// Main OnEvent handler called for events
	HRESULT __stdcall OnEvent(IPortableDeviceValues* pEventParameters);

	// Register for device events. 
	HRESULT __stdcall Register(IPortableDevice* pDevice);

	// Unregister from device event notification
	HRESULT __stdcall UnRegister(IPortableDevice* pDevice);

private:
	// Ref-counter
	ULONG   m_cRef;

	// Cookie to use while unadvising
	LPWSTR  m_pwszEventCookie;
};

// Standard QI implementation for IID_IPortableDeviceEventCallback
HRESULT __stdcall CPortableDeviceEventsCallback::QueryInterface(
	REFIID  riid,
	LPVOID* ppvObj)
{
	HRESULT hr = S_OK;
	if (ppvObj == NULL)
	{
		hr = E_INVALIDARG;
		return hr;
	}

	if ((riid == IID_IUnknown) ||
		(riid == IID_IPortableDeviceEventCallback))
	{
		AddRef();
		*ppvObj = this;
	}
	else
	{
		hr = E_NOINTERFACE;
	}

	return hr;
}

// Standard AddRef implementation
ULONG __stdcall CPortableDeviceEventsCallback::AddRef()
{
	InterlockedIncrement((long*)&m_cRef);
	return m_cRef;
}

// Standard Release implementation
ULONG __stdcall CPortableDeviceEventsCallback::Release()
{
	ULONG ulRefCount = m_cRef - 1;

	if (InterlockedDecrement((long*)&m_cRef) == 0)
	{
		delete this;
		return 0;
	}
	return ulRefCount;
}

// Register for device events. 
HRESULT __stdcall CPortableDeviceEventsCallback::Register(IPortableDevice* pDevice)
{
	//
	// Check if we are already registered. If so
	// return S_FALSE
	//
	if (m_pwszEventCookie != NULL)
	{
		return S_FALSE;
	}

	HRESULT hr = S_OK;

	CComPtr<IPortableDeviceValues> spEventParameters;
	if (hr == S_OK)
	{
		hr = CoCreateInstance(CLSID_PortableDeviceValues,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IPortableDeviceValues,
			(VOID**)&spEventParameters);
	}

	// IPortableDevice::Advise is used to register for event notifications
	// This returns a cookie (string) that is needed while unregistering
	if (hr == S_OK)
	{
		hr = pDevice->Advise(0, this, spEventParameters, &m_pwszEventCookie);
	}

	return hr;
}

// Unregister from device event notification
HRESULT __stdcall CPortableDeviceEventsCallback::UnRegister(IPortableDevice* pDevice)
{
	//
	// Return S_OK if we are not registered for any thing
	//
	if (m_pwszEventCookie == NULL)
	{
		return S_OK;
	}

	// IPortableDevice::Unadvise is used to stop event notification
	// We use the cookie (string) that we received while registering
	HRESULT hr = pDevice->Unadvise(m_pwszEventCookie);

	// Free string allocated earlier by Advise call
	CoTaskMemFree(m_pwszEventCookie);
	m_pwszEventCookie = NULL;

	return hr;
}

// Main OnEvent handler called for events
HRESULT __stdcall CPortableDeviceEventsCallback::OnEvent(
	IPortableDeviceValues* pEventParameters)
{
	HRESULT hr = S_OK;

	if (pEventParameters == NULL)
	{
		hr = E_POINTER;
	}

	// The pEventParameters collection contains information about the event that was
	// fired. We'll at least need the EVENT_ID to figure out which event was fired
	// and based on that retrieve additional values from the collection

	// Display the event that was fired
	GUID EventId;
	if (hr == S_OK)
	{
		hr = pEventParameters->GetGuidValue(WPD_EVENT_PARAMETER_EVENT_ID, &EventId);
	}

	LPWSTR pwszEventId = NULL;
	if (hr == S_OK)
	{
		hr = StringFromCLSID(EventId, &pwszEventId);
	}

	if (hr == S_OK)
	{
		printf("******** Event ********\n");
		printf("Event: %ws\n", pwszEventId);
	}

	if (pwszEventId != NULL)
	{
		CoTaskMemFree(pwszEventId);
	}

	// Display the ID of the object that was affected
	// We can also obtain WPD_OBJECT_NAME, WPD_OBJECT_PERSISTENT_UNIQUE_ID,
	// WPD_OBJECT_PARENT_ID, etc.
	LPWSTR pwszObjectId = NULL;
	if (hr == S_OK)
	{
		hr = pEventParameters->GetStringValue(WPD_OBJECT_ID, &pwszObjectId);
	}

	if (hr == S_OK)
	{
		printf("Object ID: %ws\n", pwszObjectId);
	}

	if (pwszObjectId != NULL)
	{
		CoTaskMemFree(pwszObjectId);
	}

	// Note that we intentionally do not call Release on pEventParameters since we 
	// do not own it

	return hr;
}

