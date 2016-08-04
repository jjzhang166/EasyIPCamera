
// EasyIpCamera_WinDlg.h : header file
//

#pragma once
#include "Resource.h"
#include "ServerManager.h"

// CEasyIpCamera_WinDlg dialog
class CEasyIpCamera_WinDlg : public CDialogEx
{
// Construction
public:
	CEasyIpCamera_WinDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_EASYIPCAMERA_WIN_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnStart();
	afx_msg void OnBnClickedBtnEnd();
	afx_msg LRESULT OnLog(WPARAM wParam, LPARAM lParam);
	int GetCaptureParamOnDlg(	
		int& nVCapID,
		int& nACapID,
		int& nWidth,
		int& nHeight ,
		int& nFps ,
		int& nBitrate ,
		int& nSampleRate ,
		int& nChannels ,
		int& nPort);
	void EnumLocalAVDevice();

private:
	//接口层调用管理
	CServerManager* m_pServerManager; 
	int m_nSourceType;
	int m_nVideoType;

public:
	afx_msg void OnDestroy();
	afx_msg void OnCbnSelchangeComboSource();
	afx_msg void OnCbnSelchangeComboVideo();
	afx_msg void OnCbnEditchangeComboSource();
	afx_msg void OnCbnEditchangeComboVideo();
};
