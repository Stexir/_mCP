#pragma once
static const wchar_t* DEVICEID = (L"USB\\VID_0A89&PID_0030\\7\&25C389C1&0&1");
//static const wchar_t* USERNAME
// Copy from HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\DeviceClasses
static const GUID GUID_DEVINTERFACE_LIST[] =
{
	// GUID_DEVINTERFACE_USB_DEVICE
	{ 0x50DD5230, 0xBA8A, 0x11D1,{ 0xB5, 0xFD, 0x00, 0x00, 0xF8, 0x05, 0xF5, 0x30 } }
};
