
// EasyIpCamera_Win.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CEasyIpCamera_WinApp:
// See EasyIpCamera_Win.cpp for the implementation of this class
//

class CEasyIpCamera_WinApp : public CWinApp
{
public:
	CEasyIpCamera_WinApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CEasyIpCamera_WinApp theApp;