/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
package org.easydarwin.easyipcamera.activity;

import android.annotation.TargetApi;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.IBinder;
import android.os.Process;
import android.support.annotation.Nullable;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import org.easydarwin.easyipcamera.camera.EasyIPCamera;
import org.easydarwin.easyipcamera.hw.EncoderDebugger;
import org.easydarwin.easyipcamera.util.Util;
import org.easydarwin.easyipcamera.view.StatusInfoView;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;


public class RecordService extends Service implements EasyIPCamera.IPCameraCallBack{

    private static final String TAG = "RecordService";
    private String mVideoPath;
    private MediaProjectionManager mMpmngr;
    private MediaProjection mMpj;
    private VirtualDisplay mVirtualDisplay;
    private int windowWidth;
    private int windowHeight;
    private int screenDensity;

    private Surface mSurface;
    private MediaCodec mMediaCodec;

    private WindowManager wm;

    private MediaCodec.BufferInfo mBufferInfo = new MediaCodec.BufferInfo();

    static EasyIPCamera mEasyIPCamera;
    private Thread mPushThread;
    private byte[] mPpsSps;
//    private static AudioStream mAudioStream;

    private int mChannelId = 1;
    private int mChannelState = 0;
    private int mFrameRate = 25;
    private int mBitRate;
    private Context mApplicationContext;
    private boolean codecAvailable = false;
    private byte[] mVps = new byte[255];
    private byte[] mSps = new byte[255];
    private byte[] mPps = new byte[128];
    private byte[] mMei = new byte[128];
    private byte[] mH264Buffer;
    private long timeStamp = System.currentTimeMillis();
    private boolean mIsRunning = false;
    private boolean mStartingService = false;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mMpmngr = (MediaProjectionManager) getApplicationContext().getSystemService(MEDIA_PROJECTION_SERVICE);
//        mAudioStream = new AudioStream(mEasyIPCamera);
        mApplicationContext = getApplicationContext();
        codecAvailable = false;
        createEnvironment();
    }

    private void createEnvironment() {
        mVideoPath = Environment.getExternalStorageDirectory().getPath() + "/";
        wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        windowWidth = 720;//wm.getDefaultDisplay().getWidth();
        windowHeight = 1280;//wm.getDefaultDisplay().getHeight();
        DisplayMetrics displayMetrics = new DisplayMetrics();
        wm.getDefaultDisplay().getMetrics(displayMetrics);
        screenDensity = displayMetrics.densityDpi;

//        while (windowWidth > 1080){
//            windowWidth /= 2;
//            windowHeight /=2;
//        }

        Log.d(TAG, String.format("kim createEnvironment Size=%dx%d", windowWidth, windowHeight));

        EncoderDebugger debugger = EncoderDebugger.debug(mApplicationContext, windowWidth, windowHeight);
        mSps = Base64.decode(debugger.getB64SPS(), Base64.NO_WRAP);
        mPps = Base64.decode(debugger.getB64PPS(), Base64.NO_WRAP);
        mH264Buffer = new byte[(int) (windowWidth*windowHeight*1.5)];
    }

    /**
     * 初始化编码器
     */
    private void initMediaCodec() {
        mFrameRate = 25;
        mBitRate = 1200000;
        EncoderDebugger debugger = EncoderDebugger.debug(mApplicationContext, windowWidth, windowHeight);
        mSps = Base64.decode(debugger.getB64SPS(), Base64.NO_WRAP);
        mPps = Base64.decode(debugger.getB64PPS(), Base64.NO_WRAP);
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private void startMediaCodec() {
        MediaFormat mediaFormat = MediaFormat.createVideoFormat("video/avc", windowWidth, windowHeight);
        mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, mBitRate);
        mediaFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2);
        mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, mFrameRate);
        mediaFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, 1);
        mediaFormat.setInteger(MediaFormat.KEY_CAPTURE_RATE, mFrameRate);
        mediaFormat.setInteger(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, 1000000 / mFrameRate);

        try {
            mMediaCodec = MediaCodec.createEncoderByType("video/avc");
        } catch (IOException e) {
            e.printStackTrace();
        }
        mMediaCodec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        mSurface = mMediaCodec.createInputSurface();
        codecAvailable = true;
        mMediaCodec.start();
        startPush();
    }

    /**
     * 停止编码并释放编码资源占用
     */
    private void stopMediaCodec() {
        if (mMediaCodec != null) {
            codecAvailable = false;
//            mMediaCodec.stop();
//            mMediaCodec.release();
//            mMediaCodec = null;
        }
        stopPush();
    }

    private void startPush() {
        if (mPushThread != null) return;
        mPushThread = new Thread(){
            @TargetApi(Build.VERSION_CODES.LOLLIPOP)
            @Override
            public void run() {
                Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);
//                mAudioStream.startRecord();
                while (mPushThread != null && codecAvailable) {
                    try {
                        int index = mMediaCodec.dequeueOutputBuffer(mBufferInfo, 10000);

                        if ((EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO != mChannelState) &&
                                (EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM != mChannelState)) {
                            Log.e(TAG, "RecordService startPush state error! mChannelState=" + mChannelState);
                            continue;
                        }

                        if (index == MediaCodec.INFO_TRY_AGAIN_LATER) {//请求超时
                            try {
                                // wait 10ms
                                Thread.sleep(10);
                            } catch (InterruptedException e) {
                            }
                        } else if (index >= 0) {//有效输出
                            ByteBuffer outputBuffer = mMediaCodec.getOutputBuffer(index);
                            //记录pps和sps
                            int type = outputBuffer.get(4) & 0x07;
                            if (type == 7 || type == 8) {
                                byte[] outData = new byte[mBufferInfo.size];
                                outputBuffer.get(outData);
                                mPpsSps = outData;
                            } else if (type == 5) {
                                //在关键帧前面加上pps和sps数据
                                //在关键帧前面加上pps和sps数据
                                System.arraycopy(mPpsSps, 0, mH264Buffer, 0, mPpsSps.length);
                                outputBuffer.get(mH264Buffer, mPpsSps.length, mBufferInfo.size);
                                mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, System.currentTimeMillis(), mH264Buffer, 0,mPpsSps.length+mBufferInfo.size);
                            } else {
                                outputBuffer.get(mH264Buffer, 0, mBufferInfo.size);
                                if (System.currentTimeMillis() - timeStamp >= 3000) {
                                    timeStamp = System.currentTimeMillis();
                                    if (Build.VERSION.SDK_INT >= 23) {
                                        Bundle params = new Bundle();
                                        params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
                                        mMediaCodec.setParameters(params);
                                    }
                                }
                                mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, System.currentTimeMillis(), mH264Buffer, 0,mPpsSps.length+mBufferInfo.size);
                            }

                            mMediaCodec.releaseOutputBuffer(index, false);
                        }
                    } catch (Exception e) {
                        e.printStackTrace();
                        break;
                    }
                }

                if(!codecAvailable){
                    mMediaCodec.stop();
                    mMediaCodec.release();
                    mMediaCodec = null;
                }
            }
        };
        mPushThread.start();
        startVirtualDisplay();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void stopPush(){
        Thread t = mPushThread;
        if (t != null){
            mPushThread = null;
            try {
                t.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
//        mAudioStream.stop();
        if (mMediaCodec != null) {
            mMediaCodec.stop();
            mMediaCodec.release();
            mMediaCodec = null;
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void startVirtualDisplay() {
        if (mMpj == null) {
            mMpj = mMpmngr.getMediaProjection(StreameActivity.mResultCode, StreameActivity.mResultIntent);
            StreameActivity.mResultCode = 0;
            StreameActivity.mResultIntent = null;

        }
        mVirtualDisplay = mMpj.createVirtualDisplay("record_screen", windowWidth, windowHeight, screenDensity,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR| DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC| DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION, mSurface, null, null);
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private void release() {
        Log.i(TAG, " release() ");
        if (mSurface != null){
            mSurface.release();
        }
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mIsRunning = true;
        mStartingService = true;
        String strPort = EasyApplication.getEasyApplication().getPort();
        final String strId = EasyApplication.getEasyApplication().getId();
        if(strPort == null|| strPort.isEmpty() || strId == null || strId.isEmpty()) {
            mStartingService = false;
            return START_STICKY;
        }

        final int iport = Integer.parseInt(strPort);

        if(mEasyIPCamera == null) {
            mStartingService = false;
            return START_STICKY;
        }

        mChannelId = mEasyIPCamera.registerCallback(this);
//        mAudioStream.setChannelId(mChannelId);
//        mAudioStream.startRecord();

        new Thread(new Runnable() {
            @Override
            public void run() {
                int result = -1;
                while(mIsRunning && result < 0) {
                    result = mEasyIPCamera.startup(iport, EasyIPCamera.AuthType.AUTHENTICATION_TYPE_BASIC, "", "", "", 0, mChannelId, strId.getBytes(), -1, null);
                    if(result < 0){
                        try {
                            Thread.sleep(100);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }

                    Log.d(TAG, "kim startup result="+result);
                }

                initMediaCodec();
                mStartingService = false;
            }
        }).start();

        int ret = super.onStartCommand(intent, flags, startId);
        return ret;
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @Override
    public void onDestroy() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                mIsRunning = false;
                while (mStartingService){
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }

                stopPush();
                setChannelState(0);
                mEasyIPCamera.resetChannel(mChannelId);
                int result = mEasyIPCamera.shutdown();
                Log.d(TAG, "kim shutdown result="+result);
                mEasyIPCamera.unrigisterCallback(RecordService.this);
                mEasyIPCamera = null;
                release();
                if (mMpj != null) {
                    mMpj.stop();
                }
                //super.onDestroy();
            }
        }).start();
    }

    private void setChannelState(int state){
        if(state <= 2) {
            mChannelState = state;
//            mAudioStream.setChannelState(state);
        }
    }

    @Override
    public void onIPCameraCallBack(int channelId, int channelState, byte[] mediaInfo, int userPtr) {
        //        Log.d(TAG, "kim onIPCameraCallBack, channelId="+channelId+", mChannelId="+mChannelId+", channelState="+channelState);
        if(channelId != mChannelId)
            return;

        setChannelState(channelState);

        switch(channelState){
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_ERROR:
                Log.d(TAG, "Screen Record EASY_IPCAMERA_STATE_ERROR");
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_WARN, "Screen Record EASY_IPCAMERA_STATE_ERROR");
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO:
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "Screen Record EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO");
                ByteBuffer buffer = ByteBuffer.wrap(mediaInfo);
                buffer.order(ByteOrder.LITTLE_ENDIAN);
                buffer.putInt(EasyIPCamera.VideoCodec.EASY_SDK_VIDEO_CODEC_H264);
                buffer.putInt(mFrameRate);
//                buffer.putInt(mAudioStream.getAudioEncCodec());
//                buffer.putInt(mAudioStream.getSamplingRate());
//                buffer.putInt(mAudioStream.getChannelNum());
//                buffer.putInt(mAudioStream.getBitsPerSample());
                buffer.putInt(0);
                buffer.putInt(0);
                buffer.putInt(0);
                buffer.putInt(0);

                buffer.putInt(0);//vps length
                buffer.putInt(mSps.length);
                buffer.putInt(mPps.length);
                buffer.putInt(0);
                buffer.put(mVps);
                buffer.put(mSps,0,mSps.length);
                if(mSps.length < 255) {
                    buffer.put(mVps, 0, 255 - mSps.length);
                }
                buffer.put(mPps,0,mPps.length);
                if(mPps.length < 128) {
                    buffer.put(mVps, 0, 128 - mPps.length);
                }
                buffer.put(mMei);
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM:
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "Screen Record EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM");
                startMediaCodec();
                //mAudioStream.startPush();
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM:
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "Screen Record EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM");
                stopMediaCodec();
                //mAudioStream.stopPush();
                break;
            default:
                break;
        }
    }
}
