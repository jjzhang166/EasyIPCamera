
// EasyIpCamera_WinDlg.cpp : implementation file
//

#include "stdafx.h"
#include "EasyIpCamera_Win.h"
#include "EasyIpCamera_WinDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CEasyIpCamera_WinDlg dialog




CEasyIpCamera_WinDlg::CEasyIpCamera_WinDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CEasyIpCamera_WinDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pServerManager = NULL;
	 m_nSourceType = -1;
	 m_nVideoType = -1;
}

void CEasyIpCamera_WinDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEasyIpCamera_WinDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_START, &CEasyIpCamera_WinDlg::OnBnClickedBtnStart)
	ON_BN_CLICKED(IDC_BTN_END, &CEasyIpCamera_WinDlg::OnBnClickedBtnEnd)
	ON_MESSAGE(MSG_LOG, &CEasyIpCamera_WinDlg::OnLog)
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_COMBO_SOURCE, &CEasyIpCamera_WinDlg::OnCbnSelchangeComboSource)
	ON_CBN_SELCHANGE(IDC_COMBO_VIDEO, &CEasyIpCamera_WinDlg::OnCbnSelchangeComboVideo)
	ON_CBN_EDITCHANGE(IDC_COMBO_SOURCE, &CEasyIpCamera_WinDlg::OnCbnEditchangeComboSource)
	ON_CBN_EDITCHANGE(IDC_COMBO_VIDEO, &CEasyIpCamera_WinDlg::OnCbnEditchangeComboVideo)
END_MESSAGE_MAP()


// CEasyIpCamera_WinDlg message handlers

void CEasyIpCamera_WinDlg::EnumLocalAVDevice()
{
	//枚举音视频输入设备
	DEVICE_LIST_T* camList = m_pServerManager->GetCameraList();
	DEVICE_LIST_T* micList = m_pServerManager->GetAudioInputDevList();

	CComboBox * pVideoSource = (CComboBox *)GetDlgItem(IDC_COMBO_VIDEO);
	if (pVideoSource)
	{
		pVideoSource->ResetContent();
	}
	CComboBox * pAudioSource = (CComboBox *)GetDlgItem(IDC_COMBO_AUDIO);
	if (pAudioSource)
	{
		pAudioSource->ResetContent();
	}

	DEVICE_INFO_T	*pCameraInfo = camList->pCamera;
	DEVICE_INFO_T	*pMicInfo = micList->pCamera;
	if (NULL != pCameraInfo)
	{
		while (pCameraInfo)
		{
			CString strName = (CString)pCameraInfo->friendlyName;
			pVideoSource->AddString(strName);
			//CAMERA_INFO_T	*pCameraInfo
			pCameraInfo = pCameraInfo->pNext;
		}
	}
	if (NULL != pMicInfo)
	{
		while (pMicInfo)
		{
			CString strName = (CString)pMicInfo->friendlyName;
			pAudioSource->AddString(strName);
			//CAMERA_INFO_T	*pCameraInfo
			pMicInfo = pMicInfo->pNext;		
		}
	}

	pVideoSource->SetCurSel(0);
	pAudioSource->SetCurSel(0);
}

BOOL CEasyIpCamera_WinDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	m_pServerManager = new CServerManager();
	m_pServerManager->SetMainDlg(this);

	CComboBox * pSourceType = (CComboBox *)GetDlgItem(IDC_COMBO_SOURCE);
	if (pSourceType)
	{		
		pSourceType->AddString(_T("本地音视频采集"));
		pSourceType->AddString(_T("屏幕采集"));
// 		pSourceType->AddString(_T("文件采集"));
// 		pSourceType->AddString(_T("网络RTSP流采集"));
		
		pSourceType->SetCurSel(0);
	}
	EnumLocalAVDevice();

	//设置采集参数初始化值
	CEdit* pEdit =NULL;
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_WIDTH );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("640")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_HEIGHT );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("480")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_FPS );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("25")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_SAMPLERATE );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("44100")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_AUDIOCHANNEL );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("2")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_ENC_BITRATE );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("2048")); 
	}
	pEdit = (CEdit*)GetDlgItem (IDC_EDIT_SERVERPORT );
	if(pEdit)
	{
		pEdit->SetWindowTextW(_T("8554")); 
	}
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CEasyIpCamera_WinDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CEasyIpCamera_WinDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CEasyIpCamera_WinDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CEasyIpCamera_WinDlg::GetCaptureParamOnDlg(	
	int& nVCapID,
	int& nACapID,
	int& nWidth,
	int& nHeight ,
	int& nFps ,
	int& nBitrate ,
	int& nSampleRate ,
	int& nChannels ,
	int& nPort)
{
	char szWidth[128] = {0,};
	wchar_t wszWidth[128] = {0,};
	char szHeight[128] = {0,};
	wchar_t wszHeight[128] = {0,};
	char szFPS[128] = {0,};
	wchar_t wszFPS[128] = {0,};
	char szBitrate[128] = {0,};
	wchar_t wszBitrate[128] = {0,};

	char szSampleRate[128] = {0,};
	wchar_t wszSampleRate[128] = {0,};
	char szChannels[128] = {0,};
	wchar_t wszChannels[128] = {0,};
	char szPort[128] = {0,};
	wchar_t wszPort[128] = {0,};

	CEdit* pEdit = NULL;
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_WIDTH);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszWidth, sizeof(wszWidth));
		if (wcslen(wszWidth) > 0)		
		{
			__WCharToMByte(wszWidth, szWidth, sizeof(szWidth)/sizeof(szWidth[0]));
			nWidth = atoi(szWidth);
		}
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_HEIGHT);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszHeight, sizeof(wszHeight));
		if (wcslen(wszHeight) > 0)		
		{
			__WCharToMByte(wszHeight, szHeight, sizeof(szHeight)/sizeof(szHeight[0]));
			nHeight = atoi(szHeight);
		}
	}
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_FPS);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszFPS, sizeof(wszFPS));
		if (wcslen(wszFPS) > 0)		
		{
			__WCharToMByte(wszFPS, szFPS, sizeof(szFPS)/sizeof(szFPS[0]));
			nFps = atoi(szFPS);
		}
	}
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_ENC_BITRATE);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszBitrate, sizeof(wszBitrate));
		if (wcslen(wszBitrate) > 0)		
		{
		__WCharToMByte(wszBitrate, szBitrate, sizeof(szBitrate)/sizeof(szBitrate[0]));
		nBitrate = atoi(szBitrate);
		}
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SAMPLERATE);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszSampleRate, sizeof(wszSampleRate));
		if (wcslen(wszSampleRate) > 0)		
		{
			__WCharToMByte(wszSampleRate, szSampleRate, sizeof(szSampleRate)/sizeof(szSampleRate[0]));
			nSampleRate = atoi(szSampleRate);	

		}
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_AUDIOCHANNEL);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszChannels, sizeof(wszChannels));
		if (wcslen(wszChannels) > 0)		
		{
			__WCharToMByte(wszChannels, szChannels, sizeof(szChannels)/sizeof(szChannels[0]));
			nChannels = atoi(szChannels);	
		}
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SERVERPORT);
	if (pEdit)
	{
		pEdit->GetWindowTextW(wszPort, sizeof(wszPort));
		if (wcslen(wszPort) > 0)		
		{
			__WCharToMByte(wszPort, szPort, sizeof(szPort)/sizeof(szPort[0]));
			nPort = atoi(szPort);	
		}
	}

	CComboBox * pVideoSource = (CComboBox *)GetDlgItem(IDC_COMBO_VIDEO);
	if (pVideoSource)
	{
		nVCapID = pVideoSource->GetCurSel();
	}
	CComboBox * pAudioSource = (CComboBox *)GetDlgItem(IDC_COMBO_AUDIO);
	if (pAudioSource)
	{
		nACapID = pAudioSource->GetCurSel();
	}
	return 1;
}

void CEasyIpCamera_WinDlg::OnBnClickedBtnStart()
{
	// TODO: Add your control notification handler code here
	if(m_pServerManager)
	{
		HWND hVideo= GetDlgItem(IDC_STATIC_VIDEOWND)->GetSafeHwnd();
		int nVCapID = 0;
		int nACapID = 0;
		int nWidth = 640;
		int nHeight = 480;
		int nFps = 25;
		int nBitrate = 2048;
		int nSampRate = 44100;
		int nChannels = 2;
		int nPort = 8554;
		//从界面获取采集参数设置
		GetCaptureParamOnDlg(nVCapID, nACapID, nWidth, nHeight, nFps, nBitrate, nSampRate, nChannels,nPort );
		int nSelSourceType = 0;
		CComboBox * pSourceType = (CComboBox *)GetDlgItem(IDC_COMBO_SOURCE);
		if (pSourceType)
		{
			nSelSourceType = pSourceType->GetCurSel();
		}
		char  szDataType[64];
		if (nSelSourceType == 0)
		{
			strcpy_s(szDataType, "YUY2" );
		} 
		else if(nSelSourceType == 1)
		{
			nVCapID = -1;
			strcpy_s(szDataType, "RGB24" )	;
			int nRet =m_pServerManager->GetScreenCapSize(nWidth, nHeight);
			if (nRet<1)
			{
				m_pServerManager->LogErr(_T("屏幕采集获取长宽失败，本地预览失败！"));
				return;
			}
			CEdit* pEdit = NULL;
			pEdit = (CEdit*)GetDlgItem(IDC_EDIT_WIDTH);
			if (pEdit)
			{
				CString strWidth = _T("");  
				strWidth.Format(_T("%d"), nWidth);
				pEdit->SetWindowText(strWidth);
			}

			pEdit = (CEdit*)GetDlgItem(IDC_EDIT_HEIGHT);
			if (pEdit)
			{
				CString strHeight = _T("");  
				strHeight.Format(_T("%d"), nHeight);
				pEdit->SetWindowText(strHeight);
			}
		}

		//Start capture
		int nRet = 0;
		nRet = m_pServerManager->StartCapture((SOURCE_TYPE)nSelSourceType, nVCapID , nACapID, hVideo, nWidth, nHeight, nFps, nBitrate, szDataType, nSampRate, nChannels);
		if (nRet>=0)
		{
			m_pServerManager->LogErr(_T("音视频采集成功。"));
		}
		else
		{
			m_pServerManager->LogErr(_T("音视频采集失败。"));
		}
		//Start server
		LIVE_CHANNEL_INFO_T channelInfo[MAX_CHANNELS];
		for (int nI=0; nI<MAX_CHANNELS; nI++)
		{
			sprintf(channelInfo[nI].name, "channel=%d", nI);
			channelInfo[nI].id = nI;
		}
		int nChannelMun = 1;
		nRet = m_pServerManager->StartServer(nPort, "", "", &channelInfo[0], MAX_CHANNELS);
		if (nRet>=0)
		{
			m_pServerManager->LogErr(_T("服务器开启成功。"));
			for (int nI=0; nI<MAX_CHANNELS; nI++)
			{
				CString strMsg = _T("");
				CString strName = (CString)(channelInfo[nI].name);
				strMsg.Format(_T("服务器开始流发送：rtsp://127.0.0.1:%d/%s"), nPort,   strName);
				m_pServerManager->LogErr(strMsg);
			}
		}
		else
		{
			m_pServerManager->LogErr(_T("服务器开启失败。"));
		}
	}
}

void CEasyIpCamera_WinDlg::OnBnClickedBtnEnd()
{
	// TODO: Add your control notification handler code here
	if(m_pServerManager)
	{
		m_pServerManager->StopServer();
		m_pServerManager->LogErr(_T("服务器关闭。"));

		m_pServerManager->StopCapture();
		m_pServerManager->LogErr(_T("音视频采集停止。"));
	}
}

LRESULT CEasyIpCamera_WinDlg::OnLog(WPARAM wParam, LPARAM lParam)
{
	CEdit* pLog = (CEdit*)GetDlgItem(IDC_EDIT_LOG);
	if (pLog)
	{
		CString strLog = (TCHAR*)lParam;
		CString strTime = _T("");
		CTime CurrentTime=CTime::GetCurrentTime(); 
		strTime.Format(_T("%04d/%02d/%02d %02d:%02d:%02d   "),CurrentTime.GetYear(),CurrentTime.GetMonth(),
			CurrentTime.GetDay(),CurrentTime.GetHour(),  CurrentTime.GetMinute(),
			CurrentTime.GetSecond());
		strLog = strTime + strLog + _T("\r\n");
		int nLength  =  pLog->SendMessage(WM_GETTEXTLENGTH);  
		pLog->SetSel(nLength,  nLength);  
		pLog->ReplaceSel(strLog); 
		pLog->SetFocus();
	}

	return 0;
}

void CEasyIpCamera_WinDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
	if (m_pServerManager)
	{
		m_pServerManager->StopServer();
		m_pServerManager->StopCapture();
		m_pServerManager->RealseScreenCapture();

		delete m_pServerManager;
		m_pServerManager = NULL;
	}
}

//源选择切换
void CEasyIpCamera_WinDlg::OnCbnSelchangeComboSource()
{
	CComboBox * pSourceType = (CComboBox *)GetDlgItem(IDC_COMBO_SOURCE);
	if (pSourceType)
	{
		int nSelSourceType = pSourceType->GetCurSel();
		if (m_nSourceType == nSelSourceType)
		{
			return ;
		}
		m_nSourceType = nSelSourceType;

		if (m_pServerManager)
		{
			m_pServerManager->StopScreenCapture();
		}

		if (nSelSourceType == 0)
		{
			EnumLocalAVDevice();
		} 
		else if(nSelSourceType==1)
		{
			CComboBox * pVideoSource = (CComboBox *)GetDlgItem(IDC_COMBO_VIDEO);
			if (pVideoSource)
			{
				//pVideoSource->Clear();
				pVideoSource->ResetContent();

				pVideoSource->AddString(_T("固定长宽"));
				pVideoSource->AddString(_T("自定义"));
				pVideoSource->AddString(_T("全屏"));
				//pVideoSource->AddString(_T("随鼠标移动"));
				pVideoSource->SetCurSel(0);
				int nSelScreenMode = pVideoSource->GetCurSel();
				if (m_pServerManager)
				{
					HWND hVideo= GetDlgItem(IDC_STATIC_VIDEOWND)->GetSafeHwnd();
					m_pServerManager->StartScreenCapture( hVideo, nSelScreenMode);
				}
			}
		}
	}
}

//视频源选择切换
void CEasyIpCamera_WinDlg::OnCbnSelchangeComboVideo()
{
	CComboBox * pSourceType = (CComboBox *)GetDlgItem(IDC_COMBO_SOURCE);
	if (pSourceType)
	{
		int nSelSourceType = pSourceType->GetCurSel();
		if (nSelSourceType == 1)//屏幕捕获切换
		{

			CComboBox * pVideoSource = (CComboBox *)GetDlgItem(IDC_COMBO_VIDEO);
			if (pVideoSource)
			{
				int nSelScreenMode = pVideoSource->GetCurSel();
				if (m_nVideoType == nSelScreenMode)
				{
					return ;
				}
				m_nVideoType = nSelScreenMode;
				if (m_pServerManager)
				{
					m_pServerManager->StopScreenCapture();
				}
				if (m_pServerManager)
				{
					HWND hVideo= GetDlgItem(IDC_STATIC_VIDEOWND)->GetSafeHwnd();
					m_pServerManager->StartScreenCapture( hVideo, nSelScreenMode);
				}
			}
		}
	}
}


void CEasyIpCamera_WinDlg::OnCbnEditchangeComboSource()
{
	// TODO: 在此添加控件通知处理程序代码
}


void CEasyIpCamera_WinDlg::OnCbnEditchangeComboVideo()
{
	// TODO: 在此添加控件通知处理程序代码
}
