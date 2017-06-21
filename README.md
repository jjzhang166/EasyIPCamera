# EasyIPCamera #

EasyIPCamera是由EasyDarwin团队开发的一套非常稳定、易用、支持多种平台（包括Windows/Linux 32&64，Android，ARM hisiv100/hisiv200/hisiv400等平台）的RTSP Server组件，适用于IPCamera、内网RTSP服务等小型RTSP流媒体服务器，接口调用非常简单成熟，无需关注RTSPServer中关于客户端监听接入、音视频多路复用、RTSP具体流程、RTP打包与发送等相关问题，支持多种音视频格式，再也不用像调用live555 RTSPServer那样处理整个RTSP OPTIONS/DESCRIBE/SETUP/PLAY/RTP/RTCP的复杂流程和担心内存释放的问题了！

BTW：EasyIPCamera在海思3156A芯片上的性能经过我们半年多的调试，目前已经可以稳定在4路1080P并发：

 - TCP/UDP 方式分别连接3路下，1080P 4M 定码率，音频格式G711（64K）G726（16K 24K 32K 40K）AAC(64K 96K 128K)都没问题；
 - TCP/UDP 方式分别连接4路下，1080P 4M 定码率，音频格式G711（64K）G726（16K 24K 32K 40K）AAC(64K 96K 128K)都没问题；
 - 支持Basic、Digest两种鉴权模式；


## 调用示例 ##

- **EasyIPCamera**：在不同的调用平台上，我们实现了不同的调用示例；
	1. Android：采集Android摄像头（支持前/后摄像头切换）和麦克风声音作为数据源的安卓RTSPServer；
	2. EasyIPCamera_Win：Windows采集摄像头/屏幕桌面/麦克风的音视频作为数据源的RTSPServer；
	3. EasyIPCamera_RTSP：以其他IPC硬件（海康、大华、雄迈）提供的RTSP流作为EasyIPCamera的数据源，对外提供RTSPServer功能；
	4. EasyIPCamera_SDK：以雄迈SDK为例，获取雄迈摄像头的数据流作为EasyIPCamera的数据源，对外提供RTSPServer功能；
	5. EasyIPCameraSimulator：我们基于EasyIPCamera做的一个IPC的模拟器，一个EasyIPCameraSimulator可以模拟器很多IPC，详细介绍参考：[http://blog.csdn.net/xiejiashu/article/details/64924006](http://blog.csdn.net/xiejiashu/article/details/64924006 "EasyIPCamera")
	6. EasyScreenCapture：Windows屏幕采集与直播同屏服务，详细介绍参考：[http://blog.csdn.net/xiejiashu/article/details/61211382](http://blog.csdn.net/xiejiashu/article/details/61211382)；
	7. ARM上我们提供基于海思v100/v200/v300/v400以及其他ARM芯片的摄像机芯片编码后的音视频做为数据源的RTSPServer，具体芯片调用demo示例代码请邮件至support@easydarwin.org申请；
	
	Windows编译方法，

    	Visual Studio 2010 编译：./EasyIPCamera-master/EasyIPCamera.sln

	Linux编译方法，

		chmod +x ./Buildit
		./Buildit

- **我们同时提供Windows、Linux、ARM、Android、IOS版本的EasyIPCamera库**：EasyIPCamera SDK商业使用需要经过授权才能永久使用，商业授权方案可以邮件发送至[support@easydarwin.org](mailto:support@easydarwin.org "EasyDarwin support mail")或者通过电话Tel：**13718530929** 进行更深入的技术与合作咨询；


	<table>
	<tr><td><b>支持平台</b></td><td><b>芯片</b></td><td><b>目录位置</b></td></tr>
	<tr><td>Windows</td><td>x86</td><td>./Lib/</td></tr>
	<tr><td>Windows</td><td>x64</td><td>./Lib/x64/</td></tr>
	<tr><td>Linux</td><td>x86</td><td>./Lib/</td></tr>
	<tr><td>Linux</td><td>x64</td><td>./Lib/x64/</td></tr>
	<tr><td>Android</td><td>Android</td><td>./Android/</td></tr>
	<tr><td>海思</td><td>arm-hisiv100-linux</td><td>./Lib/hisiv100/</td></tr>
	<tr><td>海思</td><td>arm-hisiv200-linux</td><td>./Lib/hisiv200/</td></tr>
	<tr><td>海思</td><td>arm-hisiv300-linux</td><td>./Lib/hisiv300/</td></tr>
	<tr><td>海思</td><td>arm-hisiv400-linux</td><td>./Lib/hisiv400/</td></tr>
	<tr><td colspan="3"><center>邮件获取更多平台版本</center></td></tr>
	</table>

## 调用全流程 ##
![](http://www.easydarwin.org/github/images/easyipcamera/easyipcamera20160805.gif)


## 设计方法 ##
EasyIPCamera参考live555 testProg中的testOnDemandRTSPServer示例程序，将一个live555 testOnDemandRTSPServer封装在一个类中，例如，我们称为Class EasyIPCamera，在EasyIPCamera_Create接口调用时，我们新建一个EasyIPCamera对象，再通过调用EasyIPCamera_Startup接口，将EasyIPCamera RTSPServer所需要的监听端口、认证信息、通道信息等参数输入到EasyIPCamera中后，EasyIPCamera就正式开始建立监听对外服务了，在服务的过程中，当有客户端的连接或断开，都会以回调事件的形式，通知给Controller调用者，调用者再具体来处理相关的回调任务，返回给EasyIPCamera，在EasyIPCamera服务的过程当中，如果回调要求需要Controller调用者提供音视频数据帧，Controller调用者可以通过EasyIPCamera_PushFrame接口，向EasyIPCamera输送具体的音视频帧数据，当调用者需要结束RTSPServer服务，只需要调用EasyIPCamera_Shutdown停止服务，再调用EasyIPCamera_Release释放EasyIPCamera就可以了，这样整个服务过程就完整了！

## Android同屏效果 ##
上方的手机运行EasyIPCamera的屏幕推送功能，下面的手机使用EasyPlayer Android版本进行播放的 同屏直播效果。网络良好的时候延迟只有200多毫秒。
<center>
<img src="http://www.easydarwin.org/images/20170223081816450.gif"  />
</center>

## 版本下载 ##

Android EasyIPCamera Demo下载： [https://fir.im/EasyIPCamera](https://fir.im/EasyIPCamera "https://fir.im/EasyIPCamera")

[![EasyIPCamera](http://www.easydarwin.org/github/images/easyipcamera/EasyIPCamera_Android.png)](https://fir.im/EasyIPCamera "EasyIPCamera")

### EasyIPCamera支持数据格式说明 ###

EASY\_SDK\_VIDEO\_FRAME\_FLAG数据可支持多种视频格式：
		
	#define EASY_SDK_VIDEO_CODEC_H265			/* H265  */
	#define EASY_SDK_VIDEO_CODEC_H264			/* H264  */
	#define	EASY_SDK_VIDEO_CODEC_MJPEG			/* MJPEG */
	#define	EASY_SDK_VIDEO_CODEC_MPEG4			/* MPEG4 */

视频帧标识支持

	#define EASY_SDK_VIDEO_FRAME_I				/* I帧 */
	#define EASY_SDK_VIDEO_FRAME_P				/* P帧 */
	#define EASY_SDK_VIDEO_FRAME_B				/* B帧 */
	#define EASY_SDK_VIDEO_FRAME_J				/* JPEG */

EASY\_SDK\_AUDIO\_FRAME\_FLAG数据可支持多种音频格式：
	
	#define EASY_SDK_AUDIO_CODEC_AAC			/* AAC */
	#define EASY_SDK_AUDIO_CODEC_G711A			/* G711 alaw*/
	#define EASY_SDK_AUDIO_CODEC_G711U			/* G711 ulaw*/
	#define EASY_SDK_AUDIO_CODEC_G726			/* G726 */


## 获取更多信息 ##

邮件：[support@easydarwin.org](mailto:support@easydarwin.org) 

WEB：[www.EasyDarwin.org](http://www.easydarwin.org)

Copyright &copy; EasyDarwin.org 2012-2017

![EasyDarwin](http://www.easydarwin.org/skin/easydarwin/images/wx_qrcode.jpg)