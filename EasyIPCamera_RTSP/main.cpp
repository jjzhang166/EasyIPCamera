#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define CALLBACK
#define Sleep(x) usleep(x*1000)
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EasyIPCameraAPI.h"
#include "EasyRTSPClientAPI.h"
#include "GetVPSSPSPPS.h"

#ifdef _WIN32
#define KEY_EASYIPCAMERA "6D72754B7A4969576B5A75413362465A706B3337634F704659584E35535642445957316C636D4666556C52545543356C654756584446616732504467523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#define KEY_EASYRTSPCLIENT "79393674363469576B5A75415561645A706C6948634F704659584E35535642445957316C636D4666556C52545543356C654756584446616732504467523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#else //x86 linux
#define KEY_EASYIPCAMERA "79393674363469576B5A7341414B5A5A706C6E59384F704659584E35535642445957316C636D4666556C52545543356C654756584446616732504467523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#define KEY_EASYRTSPCLIENT "7939367436354F576B5971417271525A706C7371634F356C59584E356158426A5957316C636D4666636E527A6346634D56714459384F424859585A7062695A4359574A76633246414D6A41784E6B566863336C4559584A33615735555A5746745A57467A65513D3D"
#endif

#define RTSP_SOURCE1 "rtsp://admin:admin@192.168.2.100/11"
#define RTSP_SOURCE2 "rtsp://admin:admin@192.168.2.100/33"

typedef enum __SOURCE_TYPE_ENUM_T
{
	SOURCE_TYPE_FILE		=	0x01,
	SOURCE_TYPE_RTSP
}SOURCE_TYPE_ENUM_T;

typedef struct __RTSP_SOURCE_CHANNEL_T
{
	int			id;
	char		name[36];
	int			pushStream;

	EASY_MEDIA_INFO_T	mediaInfo;
	Easy_RTSP_Handle	rtspHandle;

	SOURCE_TYPE_ENUM_T		sourceType;
	char	source_uri[128];
	char	username[16];
	char	password[16];
}RTSP_SOURCE_CHANNEL_T;

//RtspClient 回调
int CALLBACK __RTSPSourceCallBack( int _channelId, void *_channelPtr, int _frameType, char *pBuf, RTSP_FRAME_INFO* _frameInfo)
{
	RTSP_SOURCE_CHANNEL_T	*pChannel = (RTSP_SOURCE_CHANNEL_T *)_channelPtr;

	if (_frameType == EASY_SDK_VIDEO_FRAME_FLAG)
	{
		if (_frameInfo->codec == EASY_SDK_VIDEO_CODEC_H264 && _frameInfo->type == EASY_SDK_VIDEO_FRAME_I && pChannel->mediaInfo.u32SpsLength < 1)
		{
			pChannel->mediaInfo.u32VideoCodec = _frameInfo->codec;
			GetH264SPSandPPS(pBuf, _frameInfo->length, (char*)pChannel->mediaInfo.u8Sps, (int *)&pChannel->mediaInfo.u32SpsLength, (char *)pChannel->mediaInfo.u8Pps, (int *)&pChannel->mediaInfo.u32PpsLength);
		}
		else if (_frameInfo->codec == EASY_SDK_VIDEO_CODEC_H265 && _frameInfo->type == EASY_SDK_VIDEO_FRAME_I && pChannel->mediaInfo.u32SpsLength < 1)
		{
			pChannel->mediaInfo.u32VideoCodec = _frameInfo->codec;
			GetH265VPSandSPSandPPS(pBuf, _frameInfo->length, (char*)pChannel->mediaInfo.u8Vps, (int *)&pChannel->mediaInfo.u32VpsLength,  (char*)pChannel->mediaInfo.u8Sps, (int *)&pChannel->mediaInfo.u32SpsLength, (char *)pChannel->mediaInfo.u8Pps, (int *)&pChannel->mediaInfo.u32PpsLength);
		}

		if (pChannel->mediaInfo.u32SpsLength > 0 && pChannel->pushStream==0x01)
		{
			EASY_AV_Frame	frame;
			memset(&frame, 0x00, sizeof(EASY_AV_Frame));

			char sps[512] = {0};
			char pps[128] = {0};
			int  spslen = 0, ppslen = 0;

			if (_frameInfo->type == 0x01)
			{
				GetH264SPSandPPS(pBuf, _frameInfo->length, sps, &spslen, pps, &ppslen);
			}

			int iOffset = 0;

			for (int i=0; i<_frameInfo->length-4; i++)
			{
				unsigned char naltype = ( (unsigned char)pBuf[i+4] & 0x1F);
				if ( (unsigned char)pBuf[i+0] == 0x00 && (unsigned char)pBuf[i+1] == 0x00 &&
					 (unsigned char)pBuf[i+2] == 0x00 && (unsigned char)pBuf[i+3] == 0x01 &&
					 (naltype==0x05 || naltype==0x01))
					 //((unsigned char)pBuf[i+4] == 0x65) || ((unsigned char)pBuf[i+4] == 0x61) )
				{
					iOffset = i;
					break;
				}
			}

			//printf("SPS: %02X%02X%02X%02X%02X\n", (unsigned char)pBuf[iOffset], (unsigned char)pBuf[iOffset+1],
				//(unsigned char)pBuf[iOffset+2],(unsigned char)pBuf[iOffset+3],(unsigned char)pBuf[iOffset+4]);

			if (_frameInfo->type == 0x01)
			{
				frame.pBuffer = (Easy_U8*)(sps);
				frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
				frame.u32AVFrameLen  = spslen;
				frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
				frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
				EasyIPCamera_PushFrame(_channelId, &frame);

				frame.pBuffer = (Easy_U8*)(pps);
				frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
				frame.u32AVFrameLen  = ppslen;
				frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
				frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
				EasyIPCamera_PushFrame(_channelId, &frame);

				printf("SPS: %02X%02X%02X%02X%02X\n", (unsigned char)sps[0], (unsigned char)sps[1],
						(unsigned char)sps[2],(unsigned char)sps[3],(unsigned char)sps[4]);

				printf("SPS: %02X%02X%02X%02X%02X\n", (unsigned char)pps[0], (unsigned char)pps[1],
					(unsigned char)pps[2],(unsigned char)pps[3],(unsigned char)pps[4]);
			}

			frame.pBuffer = (Easy_U8*)(pBuf+iOffset+4);
			frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
			frame.u32AVFrameLen  = _frameInfo->length-iOffset-4;
			frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			EasyIPCamera_PushFrame(_channelId, &frame);
		}
	}
	else if (_frameType == EASY_SDK_AUDIO_FRAME_FLAG)
	{
		if (pChannel->mediaInfo.u32AudioCodec < 1)
		{
			pChannel->mediaInfo.u32AudioSamplerate = _frameInfo->sample_rate;
			pChannel->mediaInfo.u32AudioChannel	   = _frameInfo->channels;
			pChannel->mediaInfo.u32AudioCodec      = _frameInfo->codec;
		}
		else if (pChannel->pushStream==0x01)
		{
			EASY_AV_Frame	frame;
			memset(&frame, 0x00, sizeof(EASY_AV_Frame));
			frame.pBuffer = (Easy_U8*)(pBuf);
			frame.u32AVFrameFlag = EASY_SDK_AUDIO_FRAME_FLAG;
			frame.u32AVFrameLen  = _frameInfo->length;
			frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
			EasyIPCamera_PushFrame(_channelId, &frame);
		}
	}

	return 0;
}



//IPCameraSdk Callback
Easy_I32 __EasyIPCamera_Callback(Easy_I32 channelId, EASY_IPCAMERA_STATE_T channelState, EASY_MEDIA_INFO_T *_mediaInfo, void *userPtr)
{
	RTSP_SOURCE_CHANNEL_T	*pChannel = (RTSP_SOURCE_CHANNEL_T *)userPtr;

	if (channelId < 0)		return -1;

	if (channelState == EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO)
	{
		if (pChannel[channelId].sourceType == SOURCE_TYPE_FILE)		//文件
		{

		}
		else if (pChannel[channelId].sourceType == SOURCE_TYPE_RTSP)					//摄像机
		{
			if (NULL != pChannel[channelId].rtspHandle)
			{
				EasyRTSP_CloseStream(pChannel[channelId].rtspHandle);
				EasyRTSP_Deinit(&pChannel[channelId].rtspHandle);
				pChannel[channelId].rtspHandle = NULL;
			}

			pChannel[channelId].mediaInfo.u32VideoFps = 30;

			EasyRTSP_Init(&pChannel[channelId].rtspHandle);
			EasyRTSP_SetCallback(pChannel[channelId].rtspHandle, __RTSPSourceCallBack);
			EasyRTSP_OpenStream(pChannel[channelId].rtspHandle, channelId, pChannel[channelId].source_uri, EASY_RTP_OVER_TCP, 1, pChannel[channelId].username, pChannel[channelId].password, (void *)&pChannel[channelId], 1000, 0, 0, 1);
		}

		printf("[channel %d] Get media info...\n", channelId);
		for (int i=0; i<6; i++)
		{
			//注:   此处视情况而定，判断是否需有音频
			if ( pChannel[channelId].mediaInfo.u32VideoCodec > 0 )
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

		pChannel[channelId].pushStream = 0x01;
		if (pChannel[channelId].sourceType == SOURCE_TYPE_FILE)
		{
			
		}
		else if (pChannel[channelId].sourceType == SOURCE_TYPE_RTSP)
		{
		}
	}
	else if (channelState == EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM)
	{
		printf("StopStream...channel[%d]\n", channelId);

		if (pChannel[channelId].sourceType == SOURCE_TYPE_FILE)
		{

		}
		else if (pChannel[channelId].sourceType == SOURCE_TYPE_RTSP)
		{
			pChannel[channelId].pushStream = 0x00;
			EasyRTSP_CloseStream(pChannel[channelId].rtspHandle);
			EasyRTSP_Deinit(&pChannel[channelId].rtspHandle);
			pChannel[channelId].rtspHandle = NULL;
		}

		memset(&pChannel[channelId].mediaInfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
	}

	return 0;
}




int main()
{
	//初始化各通道信息
	#define	MAX_CHANNEL_NUM			2

	int rtspPort = 8554;

	RTSP_SOURCE_CHANNEL_T	channel[MAX_CHANNEL_NUM];
	memset(&channel[0], 0x00, sizeof(RTSP_SOURCE_CHANNEL_T) * MAX_CHANNEL_NUM);
	for (int i=0; i<MAX_CHANNEL_NUM; i++)
	{
		channel[i].id = i;
		sprintf(channel[i].name, "channel=%d", i+1);

		if (i==0)
		{
			channel[i].sourceType = SOURCE_TYPE_RTSP;
			strcpy(channel[i].source_uri, RTSP_SOURCE1);
			strcpy(channel[i].username, "admin");
			strcpy(channel[i].password, "admin");
		}
		else if (i==1)
		{
			channel[i].sourceType = SOURCE_TYPE_RTSP;
			strcpy(channel[i].source_uri, RTSP_SOURCE2);
			strcpy(channel[i].username, "admin");
			strcpy(channel[i].password, "admin");
		}

		printf("rtsp url: rtsp://ip:%d/%s\n", rtspPort, channel[i].name);
	}

	//进程名:  EasyIPCamera_RTSP.exe
	int activate_ret = EasyRTSP_Activate(KEY_EASYRTSPCLIENT);
	if (activate_ret < 0)
	{
		printf("激活libEasyRTSPClient失败: %d\n", activate_ret);
	}

	LIVE_CHANNEL_INFO_T	liveChannel[MAX_CHANNEL_NUM];
	memset(&liveChannel[0], 0x00, sizeof(LIVE_CHANNEL_INFO_T)*MAX_CHANNEL_NUM);
	for (int i=0; i<MAX_CHANNEL_NUM; i++)
	{
		liveChannel[i].id = channel[i].id;
		strcpy(liveChannel[i].name, channel[i].name);
	}

	activate_ret = EasyIPCamera_Activate(KEY_EASYIPCAMERA);
	if (activate_ret < 0)
	{
		printf("激活libEasyIPCamera失败: %d\n", activate_ret);
	}

	EasyIPCamera_Startup(rtspPort, AUTHENTICATION_TYPE_BASIC,"", (unsigned char*)"", (unsigned char*)"", __EasyIPCamera_Callback, (void *)&channel[0], &liveChannel[0], MAX_CHANNEL_NUM);
	//EasyIPCamera_Startup(554, NULL, NULL, __EasyIPCamera_Callback, (void *)&channel[0], &liveChannel[0], MAX_CHANNEL_NUM);


	printf("按回车键退出.\n");
	getchar();

	
	//先关闭源
	for (int i=0; i<MAX_CHANNEL_NUM; i++)
	{
		if (NULL != channel[i].rtspHandle)
		{
			EasyRTSP_CloseStream(channel[i].rtspHandle);
			EasyRTSP_Deinit(&channel[i].rtspHandle);
			channel[i].rtspHandle = NULL;
		}
	}

	//再关闭rtsp server
	EasyIPCamera_Shutdown();

	return 0;
}
