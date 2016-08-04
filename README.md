# EasyIPCamera #

EasyIPCamera是由EasyDarwin团队开发的一套非常稳定、易用、支持多种平台（包括Windows/Linux 32&64，Android，ARM hisiv100/hisiv200/hisiv400等平台）的RTSPServer组件，接口调用非常简单成熟，无需关注RTSPServer中关于客户端监听接入、音视频多路复用、RTSP具体流程、RTP打包与发送等相关问题，支持多种音视频格式，再也不用像调用live555 RTSPServer那样处理整个RTSP OPTIONS/DESCRIBE/SETUP/PLAY/RTP/RTCP的复杂流程和担心内存释放的问题了！

## 调用示例 ##

- **EasyIPCamera**：在不同的调用平台上，我们实现了不同的调用示例；
	1. EasyIPCamera_Win：Windows采集摄像头/屏幕桌面/麦克风的音视频做为数据源的RTSPServer；
	2. EasyIPCamera_RTSP：以其他IPC硬件（海康、大华、雄迈）提供的RTSP流做为EasyIPCamera的数据源，对外提供RTSPServer功能；
	3. ARM上我们提供基于海思HI3518E、HI3518C、HI3516C的摄像机芯片编码后的音视频做为数据源的RTSPServer；
	
	Windows编译方法，

    	Visual Studio 2010 编译：./EasyIPCamera-master/EasyIPCamera.sln

	Linux编译方法，

		chmod +x ./Buildit
		./Buildit

- **我们同时提供Windows、Linux、ARM版本的EasyIPCamera SDK**：可通过邮件发送至[support@easydarwin.org](mailto:support@easydarwin.org "EasyDarwin support mail")进行申请，注意ARM版本需要附上交叉编译工具链，我们会帮您具体编译，目前Github已经更新支持的平台有：

	<table>
	<tr><td><b>支持平台</b></td><td><b>芯片</b></td><td><b>目录位置</b></td></tr>
	<tr><td>Windows</td><td>x86</td><td>./Lib/</td></tr>
	<tr><td>Windows</td><td>x64</td><td>./Lib/x64/</td></tr>
	<tr><td>Linux</td><td>x86</td><td>./Lib/</td></tr>
	<tr><td>Linux</td><td>x64</td><td>./Lib/x64/</td></tr>
	<tr><td>海思</td><td>arm-hisiv100-linux</td><td>./Lib/hisiv100/</td></tr>
	<tr><td>海思</td><td>arm-hisiv200-linux</td><td>./Lib/hisiv200/</td></tr>
	<tr><td>海思</td><td>arm-hisiv400-linux</td><td>./Lib/hisiv400/</td></tr>
	<tr><td colspan="3"><center>邮件获取更多平台版本</center></td></tr>
	</table>

## 调用全流程 ##
![](http://www.easydarwin.org/skin/easydarwin/images/easyipcamera20160805.gif)


## 设计方法 ##
EasyIPCamera参考live555 testProg中的testOnDemandRTSPServer示例程序，将一个live555 testOnDemandRTSPServer封装在一个类中，例如，我们称为Class EasyIPCamera，在EasyIPCamera_Create接口调用时，我们新建一个EasyIPCamera对象，再通过调用EasyIPCamera_Startup接口，将EasyIPCamera RTSPServer所需要的监听端口、认证信息、通道信息等参数输入到EasyIPCamera中后，EasyIPCamera就正式开始建立监听对外服务了，在服务的过程中，当有客户端的连接或断开，都会以回调事件的形式，通知给Controller调用者，调用者再具体来处理相关的回调任务，返回给EasyIPCamera，在EasyIPCamera服务的过程当中，如果回调要求需要Controller调用者提供音视频数据帧，Controller调用者可以通过EasyIPCamera_PushFrame接口，向EasyIPCamera输送具体的音视频帧数据，当调用者需要结束RTSPServer服务，只需要调用EasyIPCamera_Shutdown停止服务，再调用EasyIPCamera_Release释放EasyIPCamera就可以了，这样整个服务过程就完整了！

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

Copyright &copy; EasyDarwin.org 2012-2016

![EasyDarwin](http://www.easydarwin.org/skin/easydarwin/images/wx_qrcode.jpg)