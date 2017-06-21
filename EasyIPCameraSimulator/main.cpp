/*
	Copyright (c) 2013-2014 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/

#ifdef _WIN32
#include <process.h>
#include "getopt.h"
#include <windows.h>
//#include<winsock2.h>
#else //for linux
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define CALLBACK
#define Sleep(x) usleep(x*1000)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EasyIPCameraAPI.h"
#include "GetVPSSPSPPS.h"

#ifdef _WIN32
#define KEY_EASYIPCAMERA "6D72754B7A4969576B5A734144375A5970306E4A384F5A4659584E35535642445957316C636D46546157313162474630623349755A58686C567778576F4E6A7734456468646D6C754A6B4A68596D397A595541794D4445325257467A65555268636E6470626C526C5957316C59584E35"
#elif def _ARM
#define KEY_EASYIPCAMERA "6D72754B7A502B2B7262494144375A5970306E4A384F706C59584E356158426A5957316C636D467A615731316247463062334A584446616732504467523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#else //x86 linux
#define KEY_EASYIPCAMERA "6D72754B7A4A4F576B596F4144375A5970306E4A384F706C59584E356158426A5957316C636D467A615731316247463062334A584446616732504467523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#endif

#define MAX_CHANNELS 16

char* ProgName;								//Program Name
char* ConfigIP	=	"127.0.0.1";			//Default EasyDarwin Address 183.220.236.189
char* ConfigPort=	"8554";					//Default EasyDarwin Port121.40.50.44
char* ConfigName=	"channel";	//Default RTSP Push StreamName
char* H264FileName = "./channel0.h264";
int	  OutputCount = 1;					//输出流路数

HANDLE g_FileCapThread[MAX_CHANNELS] ;
FILE * fES[MAX_CHANNELS];

typedef struct tagSOURCE_CHANNEL_T
{
	int		id;
	char		name[36];
	int		pushStream;

	EASY_MEDIA_INFO_T	mediaInfo;
	void*	anyHandle;
	bool		bThreadLiving	;	

	char		source_uri[128];
	char		username[16];
	char		password[16];
}SOURCE_CHANNEL_T;

//Globle Func for thread callback
unsigned int _stdcall  CaptureFileThread(void* lParam);

	//1.获取主机ip  
bool GetLocalIP(char* ip)  
{  
	if ( !ip )
	{
		return false;
	}
#ifdef _WIN32
	//1.初始化wsa  
	HOSTENT* host;
	WSADATA wsaData;  
	int ret=WSAStartup(MAKEWORD(2,2),&wsaData);  
	if (ret!=0)  
	{  
		return false;  
	}  
#else 
	struct hostent *host;
#endif

	//2.获取主机名  
	char hostname[256];  
	ret=gethostname(hostname,sizeof(hostname));  
	if (ret==-1)  
	{  
		return false;  
	}  
		
	host=gethostbyname(hostname);  
	if (host==0)  
	{  
		return false;  
	}  
	//4.转化为char*并拷贝返回  
	strcpy(ip,inet_ntoa(*(in_addr*)*host->h_addr_list));  

#ifdef _WIN32
	    WSACleanup( );  
#endif

	return true;  
}  

int GetAvaliblePort(int nPort)
{
#ifdef _WIN32
	//1.初始化wsa  
	HOSTENT* host;
	WSADATA wsaData;  
	int ret=WSAStartup(MAKEWORD(2,2),&wsaData);  
	if (ret!=0)  
	{  
		return -1;  
	}  
#endif

	int port = nPort;
	int fd = 0;
	sockaddr_in addr;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#ifdef _WIN32
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else 
	inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
#endif
	while(bind(fd, (sockaddr *)(&addr), sizeof(sockaddr_in)) < 0)
	{
		printf("port %d has been used.\n", port);
		port++;
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		fd = socket(AF_INET, SOCK_STREAM, 0);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
#ifdef _WIN32
		addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else 
		inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
#endif
		Sleep(1);
	}
#ifdef _WIN32
	closesocket(fd);
	WSACleanup( );  
#else
	close(fd);
#endif
	return port;
}

//IPCameraSdk Callback
Easy_I32 __EasyIPCamera_Callback(Easy_I32 channelId, EASY_IPCAMERA_STATE_T channelState, EASY_MEDIA_INFO_T *_mediaInfo, void *userPtr)
{
	SOURCE_CHANNEL_T	*pChannel = (SOURCE_CHANNEL_T *)userPtr;

	if (channelId < 0)		return -1;

	if (channelState == EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO)
	{
			if (NULL != pChannel[channelId].anyHandle)
			{
				printf("[channel %d] Get media info...\n", channelId);
				memcpy(_mediaInfo, &pChannel[channelId].mediaInfo, sizeof(EASY_MEDIA_INFO_T));
			}
	}
	else if (channelState == EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM)
	{
		printf("[channel %d] PlayStream...\n", channelId);

		pChannel[channelId].pushStream = 0x01;

	}
	else if (channelState == EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM)
	{
		printf("channel[%d] StopStream...\n", channelId);

		pChannel[channelId].pushStream = 0x00;

	}

	return 0;
}

void PrintUsage()
{
	printf("Usage:\n");
	printf("------------------------------------------------------\n");
	printf("%s [-c <outputChannels> -p <port> -n <streamName>]\n", ProgName);
	printf("Help Mode:   %s -h \n", ProgName );
	printf("For example: %s -c 8 -p 8554 -n channel \n", ProgName); 
	printf("------------------------------------------------------\n");
}


//主函数--------------------------------------------
int main(int argc, char * argv[])
{
	int isActivated = 0 ;
#ifndef _WIN32
   signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
	extern char* optarg;
#endif
	int ch;
    char szIP[16] = {0};

	for(int i=0; i<16; i++)
	{
		fES[i] = NULL;
	}

	ProgName = argv[0];
	PrintUsage();

	while ((ch = getopt(argc,argv, "hc:p:n:")) != EOF) 
	{
		switch(ch)
		{
		case 'h':
			PrintUsage();
			return 0;
			break;
		case 'c':
			OutputCount=atoi(optarg);
			break;
// 		case 'f':
// 			H264FileName = optarg;
// 			break;
// 		case 'd':
// 			ConfigIP =optarg;
// 			break;
		case 'p':
			ConfigPort =optarg;
			break;
		case 'n':
			ConfigName =optarg;
			break;
		case '?':
			return 0;
			break;
		default:
			break;
		}
	}


	int activate_ret = EasyIPCamera_Activate(KEY_EASYIPCAMERA);
	if (activate_ret < 0)
	{
		printf("激活libEasyIPCamera失败: %d\n", activate_ret);
		printf("Press Enter exit...\n");
		getchar();
		exit(1);
	}

	/*bool bSuc = GetLocalIP(szIP);
	if (!bSuc)
	{
		printf("获取本机IP失败！\n");
		printf("Press Enter exit...\n");
		getchar();
		return 0;
	}*/
	sprintf(szIP, "127.0.0.1");

	int nServerPort = GetAvaliblePort(atoi(ConfigPort));


	SOURCE_CHANNEL_T*	channels = new SOURCE_CHANNEL_T[OutputCount];
	memset(&channels[0], 0x00, sizeof(SOURCE_CHANNEL_T) * OutputCount);
	char rtsp_url[128];
	char sFileName[256] = {0,};
	for (int i=0; i<OutputCount; i++)
	{
		channels[i].id = i;
		sprintf(channels[i].name, "%s%d",  ConfigName, i);
		sprintf(rtsp_url, "rtsp://%s:%d/%s", szIP, nServerPort, channels[i].name);

		strcpy(channels[i].source_uri, rtsp_url);
		strcpy(channels[i].username, "admin");
		strcpy(channels[i].password, "admin");

		memset(&channels[i].mediaInfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
		channels[i].mediaInfo.u32VideoCodec =   EASY_SDK_VIDEO_CODEC_H264;
		channels[i].mediaInfo.u32VideoFps = 25;
		
		//打开h264文件
		sprintf(sFileName, "./%s%d.h264", ConfigName, i);
		fES[i] = fopen(sFileName, "rb");
		if (NULL == fES[i] )       
		{
			printf("打开文件:   %s 失败\n", sFileName);
			//printf("Press Enter exit...\n");
			continue;
		}
		else
		{
			channels[i].anyHandle = fES[i];

			g_FileCapThread[i] = (HANDLE)_beginthreadex(NULL, 0, CaptureFileThread, (void*)&channels[i],0,0);
			channels[i].bThreadLiving		= true;
			printf("rtspserver output stream url:	   %s........................\n", rtsp_url);
		}
	}

	LIVE_CHANNEL_INFO_T*	liveChannels = new LIVE_CHANNEL_INFO_T[OutputCount];
	memset(&liveChannels[0], 0x00, sizeof(LIVE_CHANNEL_INFO_T)*OutputCount);
	for (int i=0; i<OutputCount; i++)
	{
		liveChannels[i].id = channels[i].id;
		strcpy(liveChannels[i].name, channels[i].name);
	}
	EasyIPCamera_Startup(nServerPort, AUTHENTICATION_TYPE_BASIC,"", (unsigned char*)"", (unsigned char*)"", __EasyIPCamera_Callback, (void *)&channels[0], &liveChannels[0], OutputCount);
	//EasyIPCamera_Startup(554, NULL, NULL, __EasyIPCamera_Callback, (void *)&channel[0], &liveChannel[0], MAX_CHANNEL_NUM);

// 	for(int i=0; i<OutputCount; i++)
// 	{
// 		sprintf(sFileName, "./%s%d.h264", ConfigName, i);
// 		fES[i] = fopen(sFileName, "rb");
// 		if (NULL == fES[i] )       
// 		{
// 
// 			printf("打开文件:   %s 失败\n", sFileName);
			//printf("Press Enter exit...\n");
// 			continue;
// 		}
// 		else
// 		{
// 			channels[i].anyHandle = fES[i];

// 			g_FileCapThread[i] = (HANDLE)_beginthreadex(NULL, 0, CaptureFileThread, (void*)&channels[i],0,0);
// 			channels[i].bThreadLiving		= true;
// 		}
// 	}


    printf("Press Enter exit...\n");
    getchar();
	  getchar();
	    getchar();
		  getchar();
		    getchar();
			  getchar();

	for(int nI=0; nI<OutputCount; nI++)
	{
		channels[nI].bThreadLiving = false;
		g_FileCapThread[nI] = 0;
	}
	Sleep(200);

	//关闭rtsp server
	EasyIPCamera_Shutdown();

	for (int nCId = 0; nCId<OutputCount; nCId++)
	{
		if (NULL != channels[nCId].anyHandle)
		{
			fclose((FILE*)channels[nCId].anyHandle);
		}
	}

	delete[] channels;
	delete[] liveChannels;
    return 0;
}

//文件读取线程
unsigned int _stdcall  CaptureFileThread(void* lParam)
{
	SOURCE_CHANNEL_T* pChannelInfo = (SOURCE_CHANNEL_T*)lParam;
	if (!pChannelInfo)
	{
		return -1;
	}
	int nChannelId = pChannelInfo->id;
	EASY_MEDIA_INFO_T   mediainfo;
	int buf_size = 1024*512;
	char *pbuf = (char *) malloc(buf_size);

	int position = 0;
	int iFrameNo = 0;
	int timestamp = 0;
	FILE* fES = (FILE*)pChannelInfo->anyHandle;

	// 线程读取文件处理推送 [3/21/2017 dingshuai]
	while (pChannelInfo&&pChannelInfo->bThreadLiving)
	{
		int nReadBytes = fread(pbuf+position, 1, 1, fES);
		if (nReadBytes < 1)
		{
			if (feof(fES))
			{
				position = 0;
				fseek(fES, 0, SEEK_SET);
				continue;
			}
			break;
		}

		position ++;

		if (position > 5)
		{
			unsigned char naltype = ( (unsigned char)pbuf[position-1] & 0x1F);

			if (	(unsigned char)pbuf[position-5]== 0x00 && 
				(unsigned char)pbuf[position-4]== 0x00 && 
				(unsigned char)pbuf[position-3] == 0x00 &&
				(unsigned char)pbuf[position-2] == 0x01 &&
				(naltype == 0x07 || naltype == 0x01 || naltype == 0x05) )
			{
				int framesize = position - 5;
				naltype = (unsigned char)pbuf[4] & 0x1F;

				char sps[512] = {0};
				char pps[128] = {0};
				int  spslen = 0, ppslen = 0;

				if (naltype == 0x07)//I frame
				{
					GetH264SPSandPPS(pbuf, framesize, (char*)pChannelInfo->mediaInfo.u8Sps, 
						(int*)&pChannelInfo->mediaInfo.u32SpsLength, (char*)pChannelInfo->mediaInfo.u8Pps, (int*)&pChannelInfo->mediaInfo.u32PpsLength);
				}

				if (pChannelInfo->pushStream)
				{
					EASY_AV_Frame	frame;
					memset(&frame, 0x00, sizeof(EASY_AV_Frame));

					int iOffset = 0;
					for (int i=0; i<framesize; i++)
					{
						unsigned char naltype = ( (unsigned char)pbuf[i+4] & 0x1F);
						if ( (unsigned char)pbuf[i+0] == 0x00 && (unsigned char)pbuf[i+1] == 0x00 &&
							(unsigned char)pbuf[i+2] == 0x00 && (unsigned char)pbuf[i+3] == 0x01 &&
							(naltype==0x05 || naltype==0x01))
							//((unsigned char)pBuf[i+4] == 0x65) || ((unsigned char)pBuf[i+4] == 0x61) )
						{
							iOffset = i;
							break;
						}
					}

					frame.pBuffer = (Easy_U8*)(pbuf+iOffset+4);
					frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
					frame.u32AVFrameLen  = framesize-iOffset-4;
					frame.u32TimestampSec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
					frame.u32TimestampUsec = 0;					//注:  如果实际为ipc编码的流,则此处填写编码时间
					EasyIPCamera_PushFrame(nChannelId, &frame);
				}

#ifndef _WIN32
				usleep(40*1000);
#else
				Sleep(40);
#endif
				memmove(pbuf, pbuf+position-5, 5);
				position = 5;

				iFrameNo ++;

				//if (iFrameNo > 100000) break;
				//break;
			}
		}
	}

	free(pbuf);

	return 0;
}
