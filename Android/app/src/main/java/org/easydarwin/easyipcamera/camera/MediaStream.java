/*
	Copyright (c) 2012-2017 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/

package org.easydarwin.easyipcamera.camera;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.preference.PreferenceManager;
import android.util.Base64;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.easydarwin.easyipcamera.activity.EasyApplication;
import org.easydarwin.easyipcamera.hw.EncoderDebugger;
import org.easydarwin.easyipcamera.hw.NV21Convertor;
import org.easydarwin.easyipcamera.sw.JNIUtil;
import org.easydarwin.easyipcamera.sw.X264Encoder;
import org.easydarwin.easyipcamera.util.Util;
import org.easydarwin.easyipcamera.view.StatusInfoView;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;

public class MediaStream implements EasyIPCamera.IPCameraCallBack {

    EasyIPCamera mEasyIPCamera;
    static final String TAG = "MediaStream";
    int width = 640, height = 480;
    int framerate = 25;
    int bitrate;
    int mCameraId = Camera.CameraInfo.CAMERA_FACING_BACK;
    MediaCodec mMediaCodec;
    SurfaceHolder mSurfaceHolder;
    Camera mCamera;
    NV21Convertor mConvertor;
    private int mChannelState = 0;
//    AudioStream audioStream;
    private boolean mSWCodec;
    private X264Encoder x264;
    private Consumer mConsumer;
    private boolean isCameraBack = true;
    private int mDgree;
    private Context mApplicationContext;
    Thread pushThread;
    boolean codecAvailable = false;
    private int mChannelId = 1;
    boolean mChannelOpen = false;//是否要推送数据
    byte[] mVps = new byte[255];
    byte[] mSps = new byte[255];
    byte[] mPps = new byte[128];
    byte[] mMei = new byte[128];

    public MediaStream(Context context, SurfaceHolder holder) {
        mApplicationContext = context;
        mSurfaceHolder = holder;
        mEasyIPCamera = new EasyIPCamera();
//        audioStream = new AudioStream(mEasyIPCamera);
    }

    public void setSurfaceHolder(SurfaceHolder holder){
        mSurfaceHolder = holder;
    }

    public void setDgree(int dgree) {
        mDgree = dgree;
    }

    /**
     * 更新分辨率
     */
    public void updateResolution(int width, int height) {
        this.width = width;
        this.height = height;
    }

    /**
     * 重新开始
     */
    public void reStartStream() {
        if (mCamera == null) return;
        mCamera.stopPreview();//停掉原来摄像头的预览
        mCamera.release();//释放资源
        stopPush();
        mEasyIPCamera.unrigisterCallback(this);
        mChannelId = mEasyIPCamera.registerCallback(this);
//        audioStream.setChannelId(mChannelId);
        initEncoder();
        createCamera();
        startPreview();
    }

    public static int[] determineMaximumSupportedFramerate(Camera.Parameters parameters) {
        int[] maxFps = new int[]{0, 0};
        List<int[]> supportedFpsRanges = parameters.getSupportedPreviewFpsRange();
        for (Iterator<int[]> it = supportedFpsRanges.iterator(); it.hasNext(); ) {
            int[] interval = it.next();
            if (interval[1] > maxFps[1] || (interval[0] > maxFps[0] && interval[1] == maxFps[1])) {
                maxFps = interval;
            }
        }
        return maxFps;
    }

    public static final boolean useSWCodec() {
        return Build.VERSION.SDK_INT >= 23;
    }

    public boolean createCamera() {
        try {
            mCamera = Camera.open(mCameraId);
            Camera.Parameters parameters = mCamera.getParameters();
            int[] max = determineMaximumSupportedFramerate(parameters);
            Camera.CameraInfo camInfo = new Camera.CameraInfo();
            Camera.getCameraInfo(mCameraId, camInfo);
            int cameraRotationOffset = camInfo.orientation;
            if (mCameraId == Camera.CameraInfo.CAMERA_FACING_FRONT)
                cameraRotationOffset += 180;
            int rotate = (360 + cameraRotationOffset - mDgree) % 360;
            parameters.setRotation(rotate);
            mSWCodec = PreferenceManager.getDefaultSharedPreferences(mApplicationContext).getBoolean("key-sw-codec", useSWCodec());
            parameters.setPreviewFormat(mSWCodec ? ImageFormat.YV12 : ImageFormat.NV21);
            List<Camera.Size> sizes = parameters.getSupportedPreviewSizes();
            parameters.setPreviewSize(width, height);
            parameters.setPreviewFpsRange(max[0], max[1]);
            mCamera.setParameters(parameters);
            int displayRotation;
            displayRotation = (cameraRotationOffset - mDgree + 360) % 360;
            mCamera.setDisplayOrientation(displayRotation);
            mCamera.setPreviewDisplay(mSurfaceHolder);
            return true;
        } catch (Exception e) {
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw);
            e.printStackTrace(pw);
            String stack = sw.toString();
            destroyCamera();
            e.printStackTrace();
            Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_WARN, "Create camera failed!");
            return false;
        }
    }

    Object lock = new byte[0];

    private class TimedBuffer {
        byte[] buffer;
        long time;

        public TimedBuffer(byte[] data) {
            buffer = data;
            time = System.currentTimeMillis();
        }
    }

    private ArrayBlockingQueue<TimedBuffer> yuvs = new ArrayBlockingQueue<TimedBuffer>(5);
    private ArrayBlockingQueue<byte[]> yuv_caches = new ArrayBlockingQueue<byte[]>(10);

    class Consumer extends Thread {
        ByteBuffer[] inputBuffers;
        ByteBuffer[] outputBuffers;
        byte[] mPpsSps = new byte[0];
        int keyFrmHelperCount = 0;
        private long timeStamp = System.currentTimeMillis();

        public Consumer() {
            super("Consumer");
        }

        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
            Camera.Size previewSize = mCamera.getParameters().getPreviewSize();
            byte[] h264 = new byte[(int) (previewSize.width * previewSize.height * 3/2)];
            try {
                if(mSWCodec){
                    initSWEncoder();
                } else {
                    startMediaCodec();
                }

                while (mConsumer != null && codecAvailable) {
                    TimedBuffer tb;
//                    Log.i(TAG, String.format("cache yuv:%d", yuvs.size()));
                    tb = yuvs.take();
                    if(!codecAvailable){
                        break;
                    }

                    byte[] data = tb.buffer;
                    long stamp = tb.time;
                    int[] outLen = new int[1];
                    if (mDgree == 0) {
                        Camera.CameraInfo camInfo = new Camera.CameraInfo();
                        Camera.getCameraInfo(mCameraId, camInfo);
                        int cameraRotationOffset = camInfo.orientation;
                        if (cameraRotationOffset == 90) {
                            if (mSWCodec) {
                                Util.yuvRotate(data, 0, previewSize.width, previewSize.height, 90);
                            } else {
                                Util.yuvRotate(data, 1, previewSize.width, previewSize.height, 90);
                            }
                        } else if (cameraRotationOffset == 270) {
                            if (mSWCodec) {
                                Util.yuvRotate(data, 0, previewSize.width, previewSize.height, 270);
                            } else {
                                Util.yuvRotate(data, 1, previewSize.width, previewSize.height, 270);
                            }
                        }
                    }
                    int r = 0;
                    byte[] newBuf = null;
                    synchronized (lock) {
                        if (mSWCodec && x264 != null) {
                            byte[] keyFrm = new byte[1];
                            if (false) {
                                if (keyFrmHelperCount++ > 50) {
                                    keyFrmHelperCount = 0;
                                    keyFrm[0] = 1;
                                } else {
                                    keyFrm[0] = 0;
                                }
                            }
                            long begin = System.currentTimeMillis();
                            r = x264.encode(data, 0, h264, 0, outLen, keyFrm);
                            if (r > 0) {
                                Log.i(TAG, String.format("encode spend:%d ms. keyFrm:%d", System.currentTimeMillis() - begin, keyFrm[0]));
//                                newBuf = new byte[outLen[0]];
//                                System.arraycopy(h264, 0, newBuf, 0, newBuf.length);
                            }
                        }
                    }
                    if (mSWCodec) {
                        if (r > 0) {
                            if(mChannelState == EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM) {
                                mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, stamp, h264, 0, outLen[0]);
                            }
                        }
                    } else {
                        inputBuffers = mMediaCodec.getInputBuffers();
                        outputBuffers = mMediaCodec.getOutputBuffers();
                        int bufferIndex = mMediaCodec.dequeueInputBuffer(5000);
                        if (bufferIndex >= 0) {
                            inputBuffers[bufferIndex].clear();
                            mConvertor.convert(data, inputBuffers[bufferIndex]);
                            mMediaCodec.queueInputBuffer(bufferIndex, 0, inputBuffers[bufferIndex].position(), stamp* 1000, 0);

                            MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                            int outputBufferIndex = 0;
                            do {
                                 outputBufferIndex = mMediaCodec.dequeueOutputBuffer(bufferInfo, 0);
                                if (outputBufferIndex<0){
                                    break;
                                }
                                ByteBuffer outputBuffer = outputBuffers[outputBufferIndex];
                                //记录pps和sps
                                int type = outputBuffer.get(4) & 0x1F;

                                Log.d(TAG, String.format("type is %d", type));
                                if (type == 7 || type == 8) {
                                    byte[] outData = new byte[bufferInfo.size];
                                    outputBuffer.get(outData, 0, bufferInfo.size);
                                    mPpsSps = outData;
                                } else if (type == 5) {
                                    //在关键帧前面加上pps和sps数据
                                    System.arraycopy(mPpsSps, 0, h264, 0, mPpsSps.length);
                                    outputBuffer.get(h264, mPpsSps.length, bufferInfo.size);
                                    if(mChannelState == EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM) {
                                        mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, bufferInfo.presentationTimeUs / 1000, h264, 0, mPpsSps.length + bufferInfo.size);
                                    }
                                } else {
                                    outputBuffer.get(h264, 0, bufferInfo.size);
                                    if (System.currentTimeMillis() - timeStamp >= 3000) {
                                        timeStamp = System.currentTimeMillis();
                                        if (Build.VERSION.SDK_INT >= 23) {
                                            Bundle params = new Bundle();
                                            params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
                                            mMediaCodec.setParameters(params);
                                        }
                                    }

                                    if(mChannelState == EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM) {
                                        mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, bufferInfo.presentationTimeUs / 1000, h264, 0, bufferInfo.size);
                                    }
                                }
                                mMediaCodec.releaseOutputBuffer(outputBufferIndex, false);
                            }
                            while(outputBufferIndex >= 0);
                        }
                    }
                    yuv_caches.offer(data);

                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }finally {
                if (mMediaCodec != null) {
                    mMediaCodec.stop();
                    mMediaCodec.release();
                    mMediaCodec = null;
                }

                synchronized (lock) {
                    if (x264 != null) {
                        x264.close();
                        x264 = null;
                        codecAvailable = false;
                    }
                }
            }
        }
    }

    /**
     * 开启预览
     */
    public synchronized void startPreview() {
        if (mCamera != null) {
            yuv_caches.clear();
            yuvs.clear();
            mCamera.startPreview();
            try {
                mCamera.autoFocus(null);
            } catch (Exception e) {
                //忽略异常
                Log.i(TAG, "auto foucus fail");
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_WARN, "Camera auto foucus failed!");
            }

            int previewFormat = mCamera.getParameters().getPreviewFormat();
            Camera.Size previewSize = mCamera.getParameters().getPreviewSize();
            int size = previewSize.width * previewSize.height
                    * ImageFormat.getBitsPerPixel(previewFormat)
                    / 8;
            mCamera.addCallbackBuffer(new byte[size]);
            mCamera.setPreviewCallbackWithBuffer(previewCallback);
        }
    }

    Camera.PreviewCallback previewCallback = new Camera.PreviewCallback() {
        @Override
        public synchronized void onPreviewFrame(byte[] data, Camera camera) {
            if(data == null || camera == null){
                return;
            }

            if (!mChannelOpen || !codecAvailable || mConsumer == null) {
                camera.addCallbackBuffer(data);
                return;
            }

            if((EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO != mChannelState) &&
                    (EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM != mChannelState)){
                camera.addCallbackBuffer(data);
                return;
            }

            Camera.Size previewSize = camera.getParameters().getPreviewSize();
            if (data.length != previewSize.width * previewSize.height * 3 / 2) {
                camera.addCallbackBuffer(data);
                return;
            }

            byte[] buffer = yuv_caches.poll();
            if (buffer == null || buffer.length != data.length) {
                buffer = new byte[data.length];
            }
            yuvs.offer(new TimedBuffer(data));
            camera.addCallbackBuffer(buffer);
        }
    };

    private void initSWEncoder(){
        if (mSWCodec) {
            synchronized (lock) {
                x264 = new X264Encoder();
                int bitrate;
                if (width >= 1920) {
                    bitrate = 3200;
                } else if (width >= 1280) {
                    bitrate = 1600;
                } else if (width >= 640) {
                    bitrate = 400;
                } else {
                    bitrate = 300;
                }
                if (mDgree == 0) {
                    x264.create(height, width, 15, bitrate);
                } else {
                    x264.create(width, height, 15, bitrate);
                }
                codecAvailable = true;
            }
        }
    }

    private void x264GetSPSPPS(){
        int[] spsLen = new int[1];
        int[] ppsLen = new int[1];
        byte[] sps = new byte[1024];
        byte[] pps = new byte[1024];

        initSWEncoder();
        int iRet = x264.getSpsPps(sps, spsLen, pps, ppsLen);
        if(iRet >= 0){
            if(spsLen[0] > 0 && spsLen[0] <= 255) {
                mSps = new byte[spsLen[0]];
                System.arraycopy(sps, 0, mSps, 0, spsLen[0]);
            }
            if(ppsLen[0] > 0 && ppsLen[0] <= 255) {
                mPps = new byte[ppsLen[0]];
                System.arraycopy(pps, 0, mPps, 0, ppsLen[0]);
            }
        } else {
            Log.e(TAG, "sw get sps pps failed!");
        }

        x264.close();
    }

    /**
     * 停止预览
     */
    public synchronized void stopPreview() {
        if (mCamera != null) {
            mCamera.stopPreview();
            mCamera.setPreviewCallbackWithBuffer(null);
        }
    }

    public Camera getCamera() {
        return mCamera;
    }

    /**
     * 切换前后摄像头
     */
    public void switchCamera() {
        int cameraCount = 0;
        if (isCameraBack) {
            isCameraBack = false;
        } else {
            isCameraBack = true;
        }
        Camera.CameraInfo cameraInfo = new Camera.CameraInfo();
        cameraCount = Camera.getNumberOfCameras();//得到摄像头的个数
        for (int i = 0; i < cameraCount; i++) {
            Camera.getCameraInfo(i, cameraInfo);//得到每一个摄像头的信息
            if (mCameraId == Camera.CameraInfo.CAMERA_FACING_FRONT) {
                //现在是后置，变更为前置
                if (cameraInfo.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {//代表摄像头的方位，CAMERA_FACING_FRONT前置      CAMERA_FACING_BACK后置
                    mCamera.stopPreview();//停掉原来摄像头的预览
                    mCamera.release();//释放资源
                    mCamera = null;//取消原来摄像头
                    mCameraId = Camera.CameraInfo.CAMERA_FACING_BACK;
                    createCamera();
                    startPreview();
                    break;
                }
            } else {
                //现在是前置， 变更为后置
                if (cameraInfo.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {//代表摄像头的方位，CAMERA_FACING_FRONT前置      CAMERA_FACING_BACK后置
                    mCamera.stopPreview();//停掉原来摄像头的预览
                    mCamera.release();//释放资源
                    mCamera = null;//取消原来摄像头
                    mCameraId = Camera.CameraInfo.CAMERA_FACING_FRONT;
                    createCamera();
                    startPreview();
                    break;
                }
            }
        }
    }

    /**
     * 销毁Camera
     */
    public synchronized void destroyCamera() {
        if (mCamera != null) {
            mCamera.stopPreview();
            try {
                mCamera.release();
            } catch (Exception e) {

            }
            mCamera = null;
        }
    }

    /**
     * 初始化编码器
     */
    private void initMediaCodec() {
        framerate = 25;
        bitrate = 2 * width * height * framerate / 20;
        EncoderDebugger debugger = EncoderDebugger.debug(mApplicationContext, width, height);
        mConvertor = debugger.getNV21Convertor();

        mSps = Base64.decode(debugger.getB64SPS(), Base64.NO_WRAP);
        mPps = Base64.decode(debugger.getB64PPS(), Base64.NO_WRAP);
    }

    private void startMediaCodec() {
        EncoderDebugger debugger = EncoderDebugger.debug(mApplicationContext, width, height);
        try {
            mMediaCodec = MediaCodec.createByCodecName(debugger.getEncoderName());
            MediaFormat mediaFormat;
            if (mDgree == 0) {
                mediaFormat = MediaFormat.createVideoFormat("video/avc", height, width);
            } else {
                mediaFormat = MediaFormat.createVideoFormat("video/avc", width, height);
            }
            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
            mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, framerate);
            mediaFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,debugger.getEncoderColorFormat());
            mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            mMediaCodec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            mMediaCodec.start();
            codecAvailable = true;
        } catch (IOException e) {
            Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "start MediaCodec failed!");
            e.printStackTrace();
        }
    }

    private void initEncoder(){
        if(mSWCodec){
            x264GetSPSPPS();
        } else {
            initMediaCodec();
        }
    }

    private void startPush(){
        yuv_caches.clear();
        yuvs.clear();

        Thread t = mConsumer;
        if (t != null) {
            mConsumer = null;
            t.interrupt();
            try {
                t.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        mConsumer = new Consumer();
        mConsumer.start();
    }

    private void stopPush(){
        codecAvailable = false;

        Thread t = mConsumer;
        if (t != null) {
            t.interrupt();
            try {
                t.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            mConsumer = null;
        }
    }

    private void setChannelState(int state){
        if(state <= 2) {
            mChannelState = state;
//            audioStream.setChannelState(state);
        }
    }

    public boolean isOpen() {
        return mChannelOpen;
    }

    public void startChannel(String ip, String port, final String id) {
        final int iport = Integer.parseInt(port);

        mChannelOpen = true;

        mChannelId = mEasyIPCamera.registerCallback(this);
//        audioStream.setChannelId(mChannelId);
        if(0 != mEasyIPCamera.active(EasyApplication.getEasyApplication())){
            Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "EasyIPCamera active failed.");
            return;
        }

        int result = mEasyIPCamera.startup(iport, EasyIPCamera.AuthType.AUTHENTICATION_TYPE_BASIC,"", "", "", 0, mChannelId, id.getBytes());
        Log.d(TAG, "startup result="+result);

        initEncoder();
        Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "EasyIPCamera started");
    }

    public void stopChannel() {
        setChannelState(0);
//        audioStream.stop();
        stopPush();

        mEasyIPCamera.resetChannel(mChannelId);
        int result = mEasyIPCamera.shutdown();
        Log.d(TAG, "shutdown result="+result);

        mEasyIPCamera.unrigisterCallback(this);
        mChannelOpen = false;
    }

    public void destroyChannel() {
        if (pushThread != null) {
            pushThread.interrupt();
        }
        setChannelState(0);

        destroyCamera();
        stopPush();
        mEasyIPCamera.resetChannel(mChannelId);
        mEasyIPCamera.shutdown();
        mChannelOpen = true;
    }

    @Override
    public void onIPCameraCallBack(int channelId, int channelState, byte[] mediaInfo, int userPtr) {
//        Log.d(TAG, "kim onIPCameraCallBack, channelId="+channelId+", mChannelId="+mChannelId+", channelState="+channelState);
        if(channelId != mChannelId)
            return;

        setChannelState(channelState);

        switch(channelState){
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_ERROR:
                Log.d(TAG, "EASY_IPCAMERA_STATE_ERROR");
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_WARN, "EASY_IPCAMERA_STATE_ERROR");
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO:
                /* 媒体信息 */
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO");

                ByteBuffer buffer = ByteBuffer.wrap(mediaInfo);
                buffer.order(ByteOrder.LITTLE_ENDIAN);
                buffer.putInt(EasyIPCamera.VideoCodec.EASY_SDK_VIDEO_CODEC_H264);
                buffer.putInt(framerate);
//                buffer.putInt(audioStream.getAudioEncCodec());
//                buffer.putInt(audioStream.getSamplingRate());
//                buffer.putInt(audioStream.getChannelNum());
//                buffer.putInt(audioStream.getBitsPerSample());
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
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM");
                startPush();
//                audioStream.startRecord();

                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM:
                Util.showDbgMsg(StatusInfoView.DbgLevel.DBG_LEVEL_INFO, "EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM");
                stopPush();
//                audioStream.stop();
                break;
            default:
                break;
        }
    }
}
