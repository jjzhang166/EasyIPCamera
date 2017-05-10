/*
	Copyright (c) 2012-2017 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "GetVPSSPSPPS.h"
#include "EasyIPCameraAPI.h"
#ifdef _WIN32
#include "getopt.h"
#include <windows.h>
#else
#include "unistd.h"
#include <signal.h>
#endif
#include "NETSDK_XiongMai\netsdk.h"

#ifdef _WIN32
#define KEY "6D72754B7A4969576B5A734157424A5A7075326E7065744659584E35535642445957316C636D46665530524C4C6D56345A613558444661672F365867523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#elif define _ARM
#define KEY "6D72754B7A502B2B7262494157424A5A7075326E7065396C59584E356158426A5957316C636D466663325272556C634D5671442F7065424859585A7062695A4359574A76633246414D6A41784E6B566863336C4559584A33615735555A5746745A57467A65513D3D"
#else
#define KEY "6D72754B7A4A4F576B5971414268465A707537354A65396C59584E356158426A5957316C636D4666633252725356634D5671442F7065424859585A7062695A4359574A76633246414D6A41784E6B566863336C4559584A33615735555A5746745A57467A65513D3D"
#endif

//初始化各通道信息
#define	MAX_CHANNEL_NUM			1
int rtspPort = 8554;							//RTSP Server Port
char* ProgName;									//Program Name
char* ConfigHost = "yinma.meibu.net";			//Default Device Address
int ConfigPort = 34567;							//Default Device Port
char* ConfigUser = "admin";						//Default Device User
char* ConfigPwd = "";							//Default Device Password
int ConfigChannel = 0;							//Default Channel ID

long g_LoginHandle = 0;							//Login handle
long g_Playhandle = 0;							//Play handle

typedef struct __CHANNEL_INFO_T {
	int channelID;
	int m_pushStream;
	EASY_MEDIA_INFO_T	mediaInfo;
}CHANNEL_INFO_T;

CHANNEL_INFO_T channels[MAX_CHANNEL_NUM] = { 0 };

void PrintUsage()
{
	printf("Usage:\n");
	printf("------------------------------------------------------\n");
	printf("%s [-p <RtspPort> -D <Device Host> -P <Device Port> -N <Device User> -W <Device Password -C <Device Channel>]\n", ProgName);
	printf("Help Mode:   %s -h \n", ProgName );
	printf("For example: %s -p 8554 -D 192.168.6.231 -P 65432 -N admin\n", ProgName); 
	printf("------------------------------------------------------\n");
}

typedef struct
{
	int		nPacketType;				// 包类型,MEDIA_DATA_TYPE
	char*	pPacketBuffer;				// 缓存区地址
	unsigned int	dwPacketSize;				// 包的大小

												// 绝对时标
	int		nYear;						// 时标:年		
	int		nMonth;						// 时标:月
	int		nDay;						// 时标:日
	int		nHour;						// 时标:时
	int		nMinute;					// 时标:分
	int		nSecond;					// 时标:秒
	unsigned int 	dwTimeStamp;					// 相对时标低位，单位为毫秒	
	unsigned int   dwFrameNum;             //帧序号
	unsigned int   dwFrameRate;            //帧率
	unsigned short uWidth;              //图像宽度
	unsigned short uHeight;             //图像高度
	unsigned int   nAudioEncodeType;          //音频编码类型IMA	 9 PCM8_VWIS 12 MS_ADPCM 13 G711A 14
	unsigned int   nBitsPerSample;			//音频采样位深
	unsigned int   nSamplesPerSecond;       // 音频采样率
	unsigned int       Reserved[6];            //保留
} STDH264_PACKET_INFO;

int __stdcall RealDataCallBack_V2(long lRealHandle, const PACKET_INFO_EX *pFrame, unsigned int dwUser)
{
	CHANNEL_INFO_T* pChannel = (CHANNEL_INFO_T*)dwUser;

	switch (pFrame->nPacketType)
	{
	case VIDEO_I_FRAME:
		if (pChannel->mediaInfo.u32SpsLength < 1)
		{
			GetH264SPSandPPS(pFrame->pPacketBuffer+16, pFrame->dwPacketSize-16, (char*)pChannel->mediaInfo.u8Sps, (int *)&pChannel->mediaInfo.u32SpsLength, (char *)pChannel->mediaInfo.u8Pps, (int *)&pChannel->mediaInfo.u32PpsLength);
			pChannel->mediaInfo.u32VideoCodec = EASY_SDK_VIDEO_CODEC_H264;
		}

		if (pChannel->mediaInfo.u32SpsLength > 0 && pChannel->m_pushStream == 0x01)
		{
			EASY_AV_Frame	frame;
			memset(&frame, 0x00, sizeof(EASY_AV_Frame));

			char sps[512] = { 0 };
			char pps[128] = { 0 };
			int  spslen = 0, ppslen = 0;

			GetH264SPSandPPS(pFrame->pPacketBuffer + 16, pFrame->dwPacketSize - 16, sps, &spslen, pps, &ppslen);

			int iOffset = 0;

			for (int i = 0; i<pFrame->dwPacketSize - 4; i++)
			{
				unsigned char naltype = ((unsigned char)pFrame->pPacketBuffer[i + 4] & 0x1F);
				if ((unsigned char)pFrame->pPacketBuffer[i + 0] == 0x00 && (unsigned char)pFrame->pPacketBuffer[i + 1] == 0x00 &&
					(unsigned char)pFrame->pPacketBuffer[i + 2] == 0x00 && (unsigned char)pFrame->pPacketBuffer[i + 3] == 0x01 &&
					(naltype == 0x05 || naltype == 0x01))
				{
					iOffset = i;
					break;
				}
			}

			frame.pBuffer = (Easy_U8*)(sps);
			frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
			frame.u32AVFrameLen = spslen;
			frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			EasyIPCamera_PushFrame(pChannel->channelID, &frame);

			frame.pBuffer = (Easy_U8*)(pps);
			frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
			frame.u32AVFrameLen = ppslen;
			frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			EasyIPCamera_PushFrame(pChannel->channelID, &frame);

			frame.pBuffer = (Easy_U8*)(pFrame->pPacketBuffer + iOffset + 4);
			frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
			frame.u32AVFrameLen = pFrame->dwPacketSize - iOffset - 4;
			frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			EasyIPCamera_PushFrame(pChannel->channelID, &frame);
		}
		break;
	case VIDEO_P_FRAME:
		EASY_AV_Frame	frame;
		memset(&frame, 0x00, sizeof(EASY_AV_Frame));
		frame.pBuffer = (Easy_U8*)(pFrame->pPacketBuffer+12);
		frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
		frame.u32AVFrameLen = pFrame->dwPacketSize-12;
		frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
		frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
		EasyIPCamera_PushFrame(pChannel->channelID, &frame);
		break;
	default:
		break;
	}

	return 1;
}


//IPCameraSdk Callback
Easy_I32 __EasyIPCamera_Callback(Easy_I32 channelId, EASY_IPCAMERA_STATE_T channelState, EASY_MEDIA_INFO_T *_mediaInfo, void *userPtr)
{
	if (channelId < 0)		return -1;
	CHANNEL_INFO_T	*pChannel = (CHANNEL_INFO_T *)userPtr;

	if (channelState == EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO)
	{
		if (g_Playhandle > 0)
		{
			H264_DVR_StopRealPlay(g_Playhandle);
			g_Playhandle = 0;
		}

		H264_DVR_CLIENTINFO playstru;
		playstru.nChannel = ConfigChannel;
		playstru.nStream = 0;
		playstru.nMode = 0;
		g_Playhandle = H264_DVR_RealPlay(g_LoginHandle, &playstru);
		if (g_Playhandle <= 0)
		{
			DWORD dwErr = H264_DVR_GetLastError();
			printf("access channel%d fail, dwErr = %d", ConfigChannel, dwErr);
		}
		else
		{
			//set callback to decode receiving data
			H264_DVR_MakeKeyFrame(g_LoginHandle, ConfigChannel, 0);
			H264_DVR_SetRealDataCallBack_V2(g_Playhandle, RealDataCallBack_V2, (long)&(pChannel[channelId]));
		}

		printf("[channel %d] Get media info...\n", channelId);
		for (int i = 0; i<6; i++)
		{
			//注:   此处视情况而定，判断是否需有音频
			if (pChannel[channelId].mediaInfo.u32VideoCodec > 0)
			{
				memcpy(_mediaInfo, &pChannel[channelId].mediaInfo, sizeof(EASY_MEDIA_INFO_T));
				break;
			}
			Sleep(500);
		}
	}
	else if (channelState == EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM)
	{
		printf("PlayStream...\n");
		pChannel[channelId].m_pushStream = 1;
	}
	else if (channelState == EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM)
	{
		printf("StopStream...channel[%d]\n", channelId);

		pChannel[channelId].m_pushStream = 0x00;
		if (g_Playhandle > 0)
		{
			H264_DVR_StopRealPlay(g_Playhandle);
			g_Playhandle = 0;
		}
		memset(&pChannel[channelId].mediaInfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
	}

	return 0;
}

int main(int argc, char * argv[])
{
	int isActivated = 0;
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
	extern char* optarg;
#endif
	int ch;
	ProgName = argv[0];
	PrintUsage();

	while ((ch = getopt(argc, argv, "hd:p:n:u:")) != EOF)
	{
		switch (ch)
		{
		case 'h':
			PrintUsage();
			return 0;
			break;
		case 'p':
			rtspPort = atoi(optarg);
			break;
		case 'D':
			ConfigHost = optarg;
			break;
		case 'P':
			ConfigPort = atoi(optarg);
			break;
		case 'N':
			ConfigUser = optarg;
			break;
		case 'W':
			ConfigPwd = optarg;
			break;
		case 'C':
			ConfigChannel = atoi(optarg);
			break;
		case '?':
			return 0;
			break;
		default:
			break;
		}
	}

	memset(&channels, 0, sizeof(CHANNEL_INFO_T)*MAX_CHANNEL_NUM);
	LIVE_CHANNEL_INFO_T	liveChannel[MAX_CHANNEL_NUM];
	memset(&liveChannel[0], 0x00, sizeof(LIVE_CHANNEL_INFO_T)*MAX_CHANNEL_NUM);
	for (int i = 0; i<MAX_CHANNEL_NUM; i++)
	{
		channels[i].channelID = i;

		liveChannel[i].id = i;
		sprintf_s(liveChannel[i].name, "channel=%d", i + 1);
		printf("rtsp url: rtsp://ip:%d/%s\n", rtspPort, liveChannel[i].name);
	}	

	int activate_ret = EasyIPCamera_Activate(KEY);
	if (activate_ret < 0)
	{
		printf("激活libEasyIPCamera失败: %d\n", activate_ret);
	}
	else
	{
		EasyIPCamera_Startup(rtspPort, AUTHENTICATION_TYPE_BASIC, "", (unsigned char*)"", (unsigned char*)"", __EasyIPCamera_Callback, &channels[0], &liveChannel[0], MAX_CHANNEL_NUM);
	}

	H264_DVR_Init(NULL, NULL);
	H264_DVR_DEVICEINFO OutDev;
	int nError = 0;
	//设置尝试连接设备次数和等待时间
	H264_DVR_SetConnectTime(3000, 1);//设置尝试连接1次，等待时间3s
	g_LoginHandle = H264_DVR_Login(ConfigHost, ConfigPort, ConfigUser, ConfigPwd, &OutDev, &nError);
	if (g_LoginHandle <= 0)
	{
		int nErr = H264_DVR_GetLastError();
		if (nErr == H264_DVR_PASSWORD_NOT_VALID)
		{
			printf("Error.PwdErr");
		}
		else
		{
			printf("Error.NotFound");

		}
	}

	printf("按回车键退出.\n");
	getchar();

	//先关闭源
	if (g_LoginHandle > 0)
	{
		H264_DVR_Logout(g_LoginHandle);
	}
	H264_DVR_Cleanup();

	//再关闭rtsp server
	EasyIPCamera_Shutdown();

	return 0;
}