#include "StdAfx.h"
#include "ServerManager.h"
#include "EasyIpCamera_WinDlg.h"

CServerManager::CServerManager(void)
{
	m_pMainDlg = NULL;
	m_bOutput = FALSE;
	m_bDSCapture = FALSE;
	m_pEncConfigInfo = NULL;
	m_pFrameBuf = NULL;
	m_pVideoManager=NULL;
	m_pAudioManager=NULL;
	m_hCaptureWnd = NULL;
	m_nScreenCaptureId = -1;
	m_pScreenCaptrue = NULL;
}

CServerManager::~CServerManager(void)
{
}

void CServerManager::SetMainDlg(CEasyIpCamera_WinDlg* pMainDlg)
{
	m_pMainDlg = pMainDlg;
}

int CServerManager::StartDSCapture(int nCamId, int nAudioId,HWND hShowWnd,int nVideoWidth, int nVideoHeight, int nFps, int nBitRate, char* szDataype, int nSampleRate, int nChannel)
{
	if (m_bDSCapture)
	{
		return 0;
	}

	if(!m_pVideoManager)
	{
		m_pVideoManager = Create_VideoCapturer();
	}
	if(!m_pAudioManager)
	{
		m_pAudioManager = Create_AudioCapturer();
	}
	//设备连接配置信息结构
	memset(&m_sDevConfigInfo, 0x0, sizeof(m_sDevConfigInfo));

	BOOL bUseThread = FALSE;
	int nRet = 0;
	CString strTemp = _T("");
	//连接设备
	//1. 我们来简单配置一个设备信息
	m_sDevConfigInfo.nDeviceId = 1;
	m_sDevConfigInfo.nVideoId = nCamId;//摄像机视频捕获ID
	m_sDevConfigInfo.nAudioId = nAudioId;//音频捕获ID
	m_sDevConfigInfo.VideoInfo.nFrameRate = nFps;
	m_sDevConfigInfo.VideoInfo.nWidth = nVideoWidth;
	m_sDevConfigInfo.VideoInfo.nHeight = nVideoHeight;
	if (szDataype)
	{
		strcpy_s(m_sDevConfigInfo.VideoInfo.strDataType, 64, szDataype);
	}
	else
	{
		strcpy_s(m_sDevConfigInfo.VideoInfo.strDataType, 64, "YUY2");
	}
	m_sDevConfigInfo.VideoInfo.nRenderType = 1;
	m_sDevConfigInfo.VideoInfo.nPinType = 1;
	m_sDevConfigInfo.VideoInfo.nVideoWndId = 0;

	m_sDevConfigInfo.AudioInfo.nAudioBufferType = 4096;
	m_sDevConfigInfo.AudioInfo.nBytesPerSample = 16;
	m_sDevConfigInfo.AudioInfo.nSampleRate = nSampleRate;//44100;//
	m_sDevConfigInfo.AudioInfo.nChannaels = nChannel;
	m_sDevConfigInfo.AudioInfo.nPinType = 2;

	//初始化RGB24->I420色彩空间转换表，便于转换时查询，提高效率
	InitLookupTable();

	//x264+faac Encoder --- Init Start
	if(!m_pEncConfigInfo)
		m_pEncConfigInfo = new Encoder_Config_Info;

	m_AACEncoderManager.Init(nSampleRate, nChannel);
	m_pEncConfigInfo->nScrVideoWidth = nVideoWidth;
	m_pEncConfigInfo->nScrVideoHeight = nVideoHeight;
	m_pEncConfigInfo->nFps = nFps;
	m_pEncConfigInfo->nMainKeyFrame = 30;
	m_pEncConfigInfo->nMainBitRate = nBitRate;
	m_pEncConfigInfo->nMainEncLevel = 1;
	m_pEncConfigInfo->nMainEncQuality = 20;
	m_pEncConfigInfo->nMainUseQuality = 0;

	m_H264EncoderManager.Init(0,m_pEncConfigInfo->nScrVideoWidth,
		m_pEncConfigInfo->nScrVideoHeight,m_pEncConfigInfo->nFps,m_pEncConfigInfo->nMainKeyFrame,
		m_pEncConfigInfo->nMainBitRate,m_pEncConfigInfo->nMainEncLevel,
		m_pEncConfigInfo->nMainEncQuality,m_pEncConfigInfo->nMainUseQuality);

	byte  sps[100];
	byte  pps[100];
	long spslen=0;
	long ppslen=0;
	m_H264EncoderManager.GetSPSAndPPS(0,sps,spslen,pps,ppslen);
	memcpy(m_sps, sps,100) ;
	memcpy(m_pps, pps,100) ;
	m_spslen = spslen;
	m_ppslen = ppslen;
	//x264+faac Encoder --- Init End

	//初始化Pusher结构信息
	memset(&m_mediainfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
	m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H264;//0x1C;
	m_mediainfo.u32VideoFps = nFps;
	m_mediainfo.u32AudioCodec = EASY_SDK_AUDIO_CODEC_AAC;
	m_mediainfo.u32AudioChannel = nChannel;
	m_mediainfo.u32AudioSamplerate = nSampleRate;//44100;//
	m_mediainfo.u32AudioBitsPerSample = 16;
	m_mediainfo.u32VpsLength = 0;// Just H265 need Vps
	m_mediainfo.u32SeiLength = 0;//no sei 
	m_mediainfo.u32SpsLength = m_spslen;			/* 视频sps帧长度 */
	m_mediainfo.u32PpsLength = m_ppslen;			/* 视频pps帧长度 */
	memcpy(m_mediainfo.u8Sps, m_sps,  m_spslen);			/* 视频sps帧内容 */
	memcpy(m_mediainfo.u8Pps, m_pps, m_ppslen);				/* 视频sps帧内容 */

	//视频可用
	if (m_sDevConfigInfo.nVideoId >= 0)
	{
		HWND hWnd = hShowWnd;		
		// 2.设置视频获取显示参数
		m_pVideoManager->SetVideoCaptureData(0, m_sDevConfigInfo.nVideoId,
			hWnd,
			m_sDevConfigInfo.VideoInfo.nFrameRate,  m_sDevConfigInfo.VideoInfo.nWidth,
			m_sDevConfigInfo.VideoInfo.nHeight,     m_sDevConfigInfo.VideoInfo.strDataType, 
			m_sDevConfigInfo.VideoInfo.nRenderType, m_sDevConfigInfo.VideoInfo.nPinType, 1, bUseThread);

		m_pVideoManager->SetDShowCaptureCallback((RealDataCallback)(CServerManager::RealDataCallbackFunc), (void*)/*s_pSourceManager*/this);

		// 3.创建获取视频的图像
		nRet =m_pVideoManager->CreateCaptureGraph();
		if(nRet<=0)
		{
			m_pVideoManager->SetCaptureVideoErr(nRet);
			Release_VideoCapturer(m_pVideoManager);
			m_pVideoManager = NULL;

			strTemp.Format(_T("Video[%d]--创建基本链路失败--In StartDSCapture()."), nCamId);
			LogErr(strTemp);
			return -1;
		}
		nRet = m_pVideoManager->BulidPrivewGraph();
		if(nRet<0)
		{
			Release_VideoCapturer(m_pVideoManager);
			m_pVideoManager = NULL;

			strTemp.Format(_T("Video[%d]--连接链路失败--In StartDSCapture()."), nCamId);			
			LogErr(strTemp);
			return -1;
		}
		else
		{
			if (nRet == 2)
			{
				strTemp.Format(_T("Video[%d]--该设备不支持内部回显，将采用外部回显模式！(可能是因为没有可以进行绘制的表面)--In StartDSCapture()."), nCamId);			
				LogErr(strTemp);
			}
			m_pVideoManager->BegineCaptureThread();
		}
	}
	else
	{
		Release_VideoCapturer(m_pVideoManager)	;
		m_pVideoManager = NULL;
		LogErr(_T("当前没有使用任何视频设备!"));
	}

	//音频可用
	if (m_sDevConfigInfo.nAudioId >= 0)
	{
		m_pAudioManager->SetAudioCaptureData(m_sDevConfigInfo.nAudioId, m_sDevConfigInfo.AudioInfo.nChannaels, 
			m_sDevConfigInfo.AudioInfo.nBytesPerSample,  m_sDevConfigInfo.AudioInfo.nSampleRate, 
			m_sDevConfigInfo.AudioInfo.nAudioBufferType, m_sDevConfigInfo.AudioInfo.nPinType, 2, bUseThread);

		m_pAudioManager->SetDShowCaptureCallback((RealDataCallback)(CServerManager::RealDataCallbackFunc), (void*)this);

		nRet =m_pAudioManager->CreateCaptureGraph();
		if(nRet<=0)
		{
			strTemp.Format(_T("Audio[%d]--创建基本链路失败--In StartDSCapture()."), nAudioId);
			LogErr(strTemp);

			Release_AudioCapturer(m_pAudioManager);
			m_pAudioManager = NULL;
			return -2;
		}
		if (m_pAudioManager)
		{
			nRet = m_pAudioManager->BulidCaptureGraph();
			if(nRet<0)
			{
				strTemp.Format(_T("Audio[%d]--连接链路失败--In StartDSCapture()."), nAudioId);
				LogErr(strTemp);

				Release_AudioCapturer(m_pAudioManager);
				m_pAudioManager = NULL;
				return -2;
			}
			else
			{
				m_pAudioManager->BegineCaptureThread();	
			}
		}
	}	
	else
	{
		LogErr(_T("当前音频设备不可用!"));
	}
	return nRet;
}

//实时数据回调函数
int  CServerManager::RealDataCallbackFunc(int nDevId, unsigned char *pBuffer, int nBufSize, 
	RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster)
{

	if (!pBuffer || nBufSize <= 0)
	{
		return -1;
	}
	//转到当前实例进行处理
	CServerManager* pThis = (CServerManager*)pMaster;
	if (pThis)
	{
		pThis->DSRealDataManager(nDevId, pBuffer, nBufSize, realDataType, realDataInfo);
	}

	return 0;
}

void CServerManager::DSRealDataManager(int nDevId, unsigned char *pBuffer, int nBufSize, 
	RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo)
{
	int nVideoWidth = 640;
	int nVideoHeight = 480;
	int nFps = 25;

	nVideoWidth = m_sDevConfigInfo.VideoInfo.nWidth ;
	nVideoHeight = m_sDevConfigInfo.VideoInfo.nHeight ;
	nFps = m_sDevConfigInfo.VideoInfo.nFrameRate;

	switch (realDataType)
	{
	case REALDATA_VIDEO:
		{
			if (m_bOutput)
			{
				//YUV格式转换
				int nWidhtHeightBuf=(nVideoWidth*nVideoHeight*3)>>1;
				CString strDataType = _T("");
				strDataType = m_sDevConfigInfo.VideoInfo.strDataType;
				BYTE* pDataBuffer = NULL;
				BYTE* pDesBuffer = pBuffer;
				if (strDataType == _T("YUY2"))
				{
					pDataBuffer=new unsigned char[nWidhtHeightBuf];
					YUY2toI420(nVideoWidth,nVideoHeight,pBuffer, pDataBuffer);
					pDesBuffer = pDataBuffer;
				}
				else //默认==RGB24
				{
					pDataBuffer=new unsigned char[nWidhtHeightBuf];
					// rgb24->i420
					ConvertRGB2YUV(nVideoWidth,nVideoHeight,pBuffer, (unsigned char*)pDataBuffer);
					pDesBuffer = pDataBuffer;
				}

				int datasize=0;
				bool keyframe=false;
				//x264编码
				{
					byte*pdata = m_H264EncoderManager.Encoder(0, pDataBuffer,
						nWidhtHeightBuf, datasize, keyframe);
	
					if (datasize>0&&pdata)
					{
						EASY_AV_Frame	frame;
						memset(&frame, 0x00, sizeof(EASY_AV_Frame));

						frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
						frame.pBuffer = (Easy_U8*)pdata+4;
						frame.u32AVFrameLen =  datasize-4;
// 						long nTimeStamp = clock();
// 						frame.u32TimestampSec = nTimeStamp/1000;
// 						frame.u32TimestampUsec =  (nTimeStamp%1000)*1000;
						frame.u32VFrameType   = ( keyframe ? EASY_SDK_VIDEO_FRAME_I : EASY_SDK_VIDEO_FRAME_P);
						for (int nI=0; nI<MAX_CHANNELS; nI++)
						{
							int nRet = EasyIPCamera_PushFrame(nI,  &frame);
						}
					}
				}

				if (pDataBuffer)
				{
					delete[] pDataBuffer;
					pDataBuffer = NULL;
				}			
			}
		}
		break;
	case REALDATA_AUDIO:
		{
			if (m_bOutput)
			{
				RTSP_FRAME_INFO	frameinfo;
				memset(&frameinfo, 0x00, sizeof(RTSP_FRAME_INFO));
				int datasize=0;
				unsigned char *pAACbuf= NULL;
				//Faac Encoder
				{
					pAACbuf=m_AACEncoderManager.Encoder(pBuffer,nBufSize,datasize);	
					if(pAACbuf == NULL)
					{
						datasize = -1;
					}	
				}
				if ( datasize>0&&pAACbuf)
				{
					EASY_AV_Frame	frame;
					memset(&frame, 0x00, sizeof(EASY_AV_Frame));

					frame.u32AVFrameFlag = EASY_SDK_AUDIO_FRAME_FLAG;
					frame.pBuffer = (Easy_U8*)pAACbuf;
					frame.u32AVFrameLen =  datasize;
// 					long nTimeStamp = clock();
// 					frame.u32TimestampSec = nTimeStamp/1000;
// 					frame.u32TimestampUsec =  (nTimeStamp%1000)*1000;
					for (int nI=0; nI<MAX_CHANNELS; nI++)
					{
						int nRet = EasyIPCamera_PushFrame(nI,  &frame);
					}
				}	
			}
		}
		break;
	}
}

//开始捕获(采集)
// eSourceType==SOURCE_LOCAL_CAMERA时，nCamId有效
// eSourceType==SOURCE_RTSP_STREAM/SOURCE_ONVIF_STREAM时，szURL有效
int CServerManager::StartCapture(SOURCE_TYPE eSourceType, int nCamId, int nAudioId, HWND hCapWnd,
													int nVideoWidth, int nVideoHeight, int nFps, int nBitRate, char* szDataType,  int nSampleRate, int nChannel )
{
	if (IsInCapture())
	{
		LogErr(_T("采集正在进行中..."));
		//StopCapture();
		return -1;
	}
	int nRet = 1;
	//RTSP Source
	if (eSourceType==SOURCE_LOCAL_CAMERA || eSourceType==SOURCE_SCREEN_CAPTURE )
	{
		//DShow本地采集/屏幕采集
		nRet = StartDSCapture(nCamId, nAudioId, hCapWnd, nVideoWidth, nVideoHeight, nFps, nBitRate,  szDataType, nSampleRate, nChannel);	
	}
	else if (SOURCE_FILE_STREAM == eSourceType)
	{
	}
	else
	{
		
	}
	m_bDSCapture = TRUE;

	return nRet;
}

//停止采集
void CServerManager::StopCapture()
{
	//清除窗口关联设备
	if (m_pVideoManager)
	{
		Release_VideoCapturer(m_pVideoManager);
		m_pVideoManager = NULL;
	}
	if (m_pAudioManager)
	{
		Release_AudioCapturer(m_pAudioManager);
		m_pAudioManager = NULL;
	}
	if (m_pEncConfigInfo)
	{
		delete m_pEncConfigInfo;
		m_pEncConfigInfo = NULL;
	}
	m_H264EncoderManager.Clean();
	m_AACEncoderManager.Clean();

	m_bDSCapture = FALSE;
}

int __EasyIPCamera_Callback(Easy_I32 channelId, EASY_IPCAMERA_STATE_T channelState, EASY_MEDIA_INFO_T *mediaInfo, void *userPtr)
{
	// 	1. 调用 EasyIPCamera_Startup 设置监听端口、回调函数和自定义数据指针
	// 		2. 启动后，程序进入监听状态
	// 		2.1		接收到客户端请求, 回调 状态:EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO          上层程序在填充完mediainfo后，返回0, 则EasyIpCamera响应客户端ok
	// 		2.2		EasyIPCamera回调状态 EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM , 则表示rtsp交互完成, 开始发送流, 上层程序调用EasyIpCamera_PushFrame 发送帧数据
	// 		2.3		EasyIPCamera回调状态 EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM , 则表示客户端已发送teaardown, 要求停止发送帧数据
	// 		3.	调用 EasyIpCamera_Shutdown(), 关闭EasyIPCamera，释放相关资源
	CServerManager* pMaster = (CServerManager*)userPtr;
	if (EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO == channelState)
	{
		EASY_MEDIA_INFO_T tempMediaInfo = pMaster->GetMediaInfo();
		memcpy(mediaInfo, &tempMediaInfo,sizeof(EASY_MEDIA_INFO_T));
	}
	else if (EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM == channelState)
	{
		pMaster->SetCanOutput (TRUE);
	}
	else if (EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM == channelState)
	{
		pMaster->SetCanOutput(FALSE ) ;
	}

	return 0;
}


//开始RTSP服务
int CServerManager::StartServer(int listenport, char *username, char *password, LIVE_CHANNEL_INFO_T *channelInfo, Easy_U32 channelNum)
{
	EasyIPCamera_Activate("6D72754B7A4969576B5A7341344B6859703163667065744659584E35535642445957316C636D466656326C754C6D56345A577858444661672F365867523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D");
	//EasyIPCamera_Callback callback, void *userptr,
	int ret = EasyIPCamera_Startup(listenport, AUTHENTICATION_TYPE_BASIC, (char*)"", (Easy_U8*)username, (Easy_U8*)password, __EasyIPCamera_Callback, this, channelInfo, channelNum);

	return ret;
}

void CServerManager::StopServer()
{
	EasyIPCamera_Shutdown();
}

void CServerManager::LogErr(CString strLog)
{
	if(!strLog.IsEmpty())
	{
		TCHAR* szLog = new TCHAR[strLog.GetLength()+1];
		StrCpy(szLog, strLog);
		if(m_pMainDlg)
			m_pMainDlg->SendMessage(MSG_LOG, 0, (LPARAM)szLog);

		delete[] szLog;
		szLog = NULL;
	}
}

//屏幕采集
int CServerManager::StartScreenCapture(HWND hShowWnd, int nCapMode)
{
	if (!m_pScreenCaptrue)
	{
		//实例化屏幕捕获管理类指针
		m_pScreenCaptrue =  CCaptureScreen::Instance(m_nScreenCaptureId);
		if (!m_pScreenCaptrue)
		{
			return -1;
		}
		m_pScreenCaptrue->SetCaptureScreenCallback((CaptureScreenCallback)&CServerManager::CaptureScreenCallBack, this);
	}
	if (m_pScreenCaptrue->IsInCapturing())
	{
		return -1;
	}
	return m_pScreenCaptrue->StartScreenCapture(hShowWnd, nCapMode);
}

void CServerManager::StopScreenCapture()
{
	if (m_pScreenCaptrue)
	{
		if (m_pScreenCaptrue->IsInCapturing())
		{
			m_pScreenCaptrue->StopScreenCapture();
		}
	}
}

void CServerManager::RealseScreenCapture()
{
	if (m_pScreenCaptrue)
	{
		CCaptureScreen::UnInstance(m_nScreenCaptureId);
		m_pScreenCaptrue = NULL;
	}
}

int CServerManager::GetScreenCapSize(int& nWidth, int& nHeight)
{
	if (m_pScreenCaptrue)
	{
		if (m_pScreenCaptrue->IsInCapturing())
		{
			m_pScreenCaptrue->GetCaptureScreenSize(nWidth, nHeight );
			return 1;
		}
		else
			return -1;
	}
	return -1;
}

int CALLBACK CServerManager::CaptureScreenCallBack(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster)
{
	if (!pBuffer || nBufSize <= 0)
	{
		return -1;
	}

	//转到当前实例进行处理
	CServerManager* pThis = (CServerManager*)pMaster;
	if (pThis)
	{
		pThis->CaptureScreenManager(nId, pBuffer, nBufSize,  realDataType, realDataInfo);
	}
	return 1;
}

void CServerManager::CaptureScreenManager(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo)
{
	ScreenCapDataInfo* pDataInfo = (ScreenCapDataInfo*)realDataInfo;

	DSRealDataManager(nId, pBuffer,nBufSize,  realDataType, realDataInfo );
}