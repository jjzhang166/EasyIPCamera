
// RTSPServer manager class [7/18/2016 SwordTwelve]
#pragma once

//DShow音视频采集库
#include "./DShowCapture/DShowCaptureAudioAndVideo_Interface.h"
#include "./EasyEncoder/FAACEncoder.h"
#include "./EasyEncoder/H264Encoder.h"
#include "./EasyEncoder/H264EncoderManager.h"
//RTSPServer support
#include "../../Include/EasyIPCameraAPI.h"
#pragma comment(lib, "../../Lib/libEasyIPCamera.lib")
#include "YUVTransform.h"
#include "CaptureScreen.h"

#define MSG_LOG 1001
#define MAX_CHANNELS 6

typedef enum tagSOURCE_TYPE
{
	SOURCE_LOCAL_CAMERA = 0,//本地音视频
	SOURCE_SCREEN_CAPTURE =1,//屏幕捕获
	SOURCE_FILE_STREAM = 2,       //文件流推送(mp4,ts,flv???)
	SOURCE_RTSP_STREAM=3,//RTSP流
	// 	//SOURCE_ONVIF_STREAM=4,//Onvif流

}SOURCE_TYPE;

class CEasyIpCamera_WinDlg;

class CServerManager
{
public:
	CServerManager(void);
	~CServerManager(void);

public:
	//开始捕获(采集)
	int StartCapture(SOURCE_TYPE eSourceType, int nCamId, int nAudioId,  HWND hCapWnd, 
		int nVideoWidth=640, int nVideoHeight=480, int nFps=25, int nBitRate=2048, char* szDataType = "YUY2",  //VIDEO PARAM
		int nSampleRate=44100, int nChannel=2 );//AUDIO PARAM
	//停止采集
	void StopCapture();
	//开始RTSP服务
	int StartServer(int listenport, char *username, char *password,  LIVE_CHANNEL_INFO_T *channelInfo, Easy_U32 channelNum);
	void StopServer();

	int StartDSCapture(int nCamId, int nAudioId,HWND hShowWnd,int nVideoWidth, int nVideoHeight, int nFps, int nBitRate, char* szDataype, int nSampleRate, int nChannel);
	static int CALLBACK RealDataCallbackFunc(int nDevId, unsigned char *pBuffer, int nBufSize, 
		RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster);
	void DSRealDataManager(int nDevId, unsigned char *pBuffer, int nBufSize, 
		RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo);
	
	//屏幕采集相关
	int StartScreenCapture(HWND hShowWnd, int nCapMode);
	void StopScreenCapture();
	void RealseScreenCapture();
	int GetScreenCapSize(int& nWidth, int& nHeight);
	//回调函数
	static int CALLBACK CaptureScreenCallBack(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster);
	void CaptureScreenManager(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo);


	BOOL IsInCapture()
	{
		return m_bDSCapture;
	}
	void SetCanOutput(BOOL bOutput)
	{
		m_bOutput = bOutput;
	}

	EASY_MEDIA_INFO_T GetMediaInfo()
	{
		return m_mediainfo;
	}

	DEVICE_LIST_T* GetAudioInputDevList()
	{
		if(!m_pAudioManager)
		{
			m_pAudioManager = Create_AudioCapturer();
		}

		if (m_pAudioManager)
		{
			return m_pAudioManager->GetAudioInputDevList();
		}
		return NULL;

	}

	DEVICE_LIST_T* GetCameraList()
	{
		if(!m_pVideoManager)
		{
			m_pVideoManager = Create_VideoCapturer();
		}
		if (m_pVideoManager)
		{
			return m_pVideoManager->GetCameraList();
		}
		return NULL;
	}

	void LogErr(CString strLog);
	void SetMainDlg(CEasyIpCamera_WinDlg* pMainDlg);

private:
	//屏幕捕获管理
	CCaptureScreen* m_pScreenCaptrue;
	HWND m_hCaptureWnd;
	int m_nScreenCaptureId;

	CEasyIpCamera_WinDlg* m_pMainDlg;
	//视频设备控制实例
	LPVideoCapturer m_pVideoManager;
	//音频设备控制实例
	LPAudioCapturer m_pAudioManager;
	//本地Dshow捕获参数设置
	DEVICE_CONFIG_INFO m_sDevConfigInfo;
	EASY_MEDIA_INFO_T   m_mediainfo;

	// x264+faac Encoder
	//AAC编码器
	FAACEncoder m_AACEncoderManager;
	//H264编码器
	CH264EncoderManager m_H264EncoderManager;
	//编码信息配置
	Encoder_Config_Info*	m_pEncConfigInfo;
	byte m_sps[100];
	byte  m_pps[100];
	long m_spslen;
	long m_ppslen;
	byte* m_pFrameBuf; 

	//是否开始向客户端输送数据
	BOOL m_bOutput;
	BOOL m_bDSCapture;

};

