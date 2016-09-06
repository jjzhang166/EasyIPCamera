/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
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
import android.util.Base64;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.easydarwin.easyipcamera.activity.EasyApplication;
import org.easydarwin.easyipcamera.hw.EncoderDebugger;
import org.easydarwin.easyipcamera.hw.NV21Convertor;
import org.easydarwin.easyipcamera.util.Util;
import org.easydarwin.easyipcamera.view.StatusInfoView;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.List;

/**
 * Created by Kim on 8/8/2016.
 */
public class MediaStream implements EasyIPCamera.IPCameraCallBack {

    EasyIPCamera mEasyIPCamera;
    static final String TAG = "MediaStream";
    int width = 640, height = 480;
    int framerate = 25;
    int bitrate;
    int mCameraId = Camera.CameraInfo.CAMERA_FACING_BACK;
    MediaCodec mMediaCodec;
    SurfaceView mSurfaceView;
    SurfaceHolder mSurfaceHolder;
    Camera mCamera;
    NV21Convertor mConvertor;
    private int mChannelState = 0;
    AudioStream audioStream;
    private boolean isCameraBack = true;
    private int mDgree;
    private Context mApplicationContext;
    Thread pushThread;
    boolean codecAvailable = false;
    private int mChannelId = 1;
    byte[] mVps = new byte[128];
    byte[] mSps = new byte[128];
    byte[] mPps = new byte[36];
    byte[] mMei = new byte[36];

    public MediaStream(Context context, SurfaceView mSurfaceView) {
        mApplicationContext = context;
        this.mSurfaceView = mSurfaceView;
        mSurfaceHolder = mSurfaceView.getHolder();
        mEasyIPCamera = new EasyIPCamera();
        audioStream = new AudioStream(mEasyIPCamera);
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
        stopMediaCodec();
        mEasyIPCamera.unrigisterCallback(this);
        mChannelId = mEasyIPCamera.registerCallback(this);
        audioStream.setChannelId(mChannelId);
        initMediaCodec();
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
            parameters.setPreviewFormat(ImageFormat.NV21);
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
            StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Warn", "Create camera failed!"));
            return false;
        }
    }

    /**
     * 开启预览
     */
    public synchronized void startPreview() {
        if (mCamera != null) {
            mCamera.startPreview();
            try {
                mCamera.autoFocus(null);
            } catch (Exception e) {
                //忽略异常
                Log.i(TAG, "auto foucus fail");
                StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Warn", "Camera auto foucus failed!"));
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

    static int test = 0;
    Camera.PreviewCallback previewCallback = new Camera.PreviewCallback() {
        ByteBuffer[] inputBuffers;
        byte[] dst;
        ByteBuffer[] outputBuffers;
        byte[] mPpsSps = new byte[0];

        @Override
        public synchronized void onPreviewFrame(byte[] data, Camera camera) {
            if (data == null || !codecAvailable) {
                mCamera.addCallbackBuffer(data);
                return;
            }

            if((EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO != mChannelState) &&
                    (EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM != mChannelState)){
                mCamera.addCallbackBuffer(data);
                return;
            }

            Camera.Size previewSize = mCamera.getParameters().getPreviewSize();
            if (data.length != previewSize.width * previewSize.height * 3 / 2) {
                mCamera.addCallbackBuffer(data);
                return;
            }

            inputBuffers = mMediaCodec.getInputBuffers();
            outputBuffers = mMediaCodec.getOutputBuffers();
            dst = new byte[data.length];
            if (mDgree == 0) {
                Camera.CameraInfo camInfo = new Camera.CameraInfo();
                Camera.getCameraInfo(mCameraId, camInfo);
                int cameraRotationOffset = camInfo.orientation;
                if (cameraRotationOffset == 0)
                    dst = data;
                if (cameraRotationOffset == 90)
                    dst = Util.rotateNV21Degree90(data, previewSize.width, previewSize.height);
                if (cameraRotationOffset == 180)
                    dst = Util.rotateNV21Degree90(data, previewSize.width, previewSize.height);
                if (cameraRotationOffset == 270)
                    dst = Util.rotateNV21Negative90(data, previewSize.width, previewSize.height);
            } else {
                dst = data;
            }
            try {
                int bufferIndex = mMediaCodec.dequeueInputBuffer(5000000);
                if (bufferIndex >= 0) {
                    inputBuffers[bufferIndex].clear();
                    mConvertor.convert(dst, inputBuffers[bufferIndex]);
                    mMediaCodec.queueInputBuffer(bufferIndex, 0, inputBuffers[bufferIndex].position(), System.nanoTime() / 1000, 0);

                    MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                    int outputBufferIndex = mMediaCodec.dequeueOutputBuffer(bufferInfo, 0);
                    while (outputBufferIndex >= 0) {
                        ByteBuffer outputBuffer = outputBuffers[outputBufferIndex];
                        byte[] outData = new byte[bufferInfo.size];
                        outputBuffer.get(outData);

//                        String data0 = String.format("%x %x %x %x %x %x %x %x %x %x ", outData[0], outData[1], outData[2], outData[3], outData[4], outData[5], outData[6], outData[7], outData[8], outData[9]);
//                        Log.e("out_data", data0);

                        //记录pps和sps
                        int type = outData[4] & 0x07;
                        if (type == 7 || type == 8) {
                            mPpsSps = outData;
                        }else if (type == 5) {
                            //在关键帧前面加上pps和sps数据
                            byte[] iframeData = new byte[mPpsSps.length + outData.length];
                            System.arraycopy(mPpsSps, 0, iframeData, 0, mPpsSps.length);
                            System.arraycopy(outData, 0, iframeData, mPpsSps.length, outData.length);
                            outData = iframeData;
                        }

                        if(mChannelState == EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM) {
                            int result = mEasyIPCamera.pushFrame(mChannelId, EasyIPCamera.FrameFlag.EASY_SDK_VIDEO_FRAME_FLAG, System.currentTimeMillis(), outData);
                            //Log.d(TAG, "kim pushFrame result="+result+", frame length="+outData.length+", type="+type);
                        }
                        mMediaCodec.releaseOutputBuffer(outputBufferIndex, false);
                        outputBufferIndex = mMediaCodec.dequeueOutputBuffer(bufferInfo, 0);
                    }

                } else {
                    Log.e(TAG, "No buffer available !");
                }
            } catch (Exception e) {
                StringWriter sw = new StringWriter();
                PrintWriter pw = new PrintWriter(sw);
                e.printStackTrace(pw);
                String stack = sw.toString();
                Log.e("save_log", stack);
                e.printStackTrace();
            } finally {
                mCamera.addCallbackBuffer(dst);
            }

            if(!codecAvailable){
                mMediaCodec.stop();
                mMediaCodec.release();
                mMediaCodec = null;
            }
        }

    };

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
            mediaFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    debugger.getEncoderColorFormat());
            mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            mMediaCodec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            mMediaCodec.start();
            codecAvailable = true;
        } catch (IOException e) {
            String info = String.format("start MediaCodec failed!");
            StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Info", info));
            e.printStackTrace();
        }
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
    }

    private void setChannelState(int state){
        if(state <= 2) {
            mChannelState = state;
            audioStream.setChannelState(state);
        }
    }

    public void startStream(String ip, String port, final String id) {
        final int iport = Integer.parseInt(port);

        mChannelId = mEasyIPCamera.registerCallback(this);
        audioStream.setChannelId(mChannelId);

        int result = mEasyIPCamera.startup(iport, EasyIPCamera.AuthType.AUTHENTICATION_TYPE_BASIC,"", "", "", 0, mChannelId, id.getBytes());
        Log.d(TAG, "kim startup result="+result);

        initMediaCodec();

        String info = String.format("EasyIPCamera started");
        StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Info", info));
    }

    public void stopStream() {
        setChannelState(0);
        audioStream.stop();
        stopMediaCodec();

        mEasyIPCamera.resetChannel(mChannelId);
        int result = mEasyIPCamera.shutdown();
        Log.d(TAG, "kim shutdown result="+result);

        mEasyIPCamera.unrigisterCallback(this);
    }

    public void destroyStream() {
        if (pushThread != null) {
            pushThread.interrupt();
        }
        setChannelState(0);

        destroyCamera();
        stopMediaCodec();
        mEasyIPCamera.resetChannel(mChannelId);
        mEasyIPCamera.shutdown();
    }

    @Override
    public void onIPCameraCallBack(int channelId, int channelState, byte[] mediaInfo, int userPtr) {
//        Log.d(TAG, "kim onIPCameraCallBack, channelId="+channelId+", mChannelId="+mChannelId+", channelState="+channelState);
        if(channelId != mChannelId)
            return;

        setChannelState(channelState);

        switch(channelState){
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_ERROR:
                Log.d(TAG, "kim EASY_IPCAMERA_STATE_ERROR");
                StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Warn", "EASY_IPCAMERA_STATE_ERROR"));
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO:
//                /* 媒体信息 */
                StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Info", "EASY_IPCAMERA_STATE_REQUEST_MEDIA_INFO"));

                ByteBuffer buffer = ByteBuffer.wrap(mediaInfo);
                buffer.order(ByteOrder.LITTLE_ENDIAN);
                buffer.putInt(EasyIPCamera.VideoCodec.EASY_SDK_VIDEO_CODEC_H264);
                buffer.putInt(framerate);
                buffer.putInt(audioStream.getAudioEncCodec());
                buffer.putInt(audioStream.getSamplingRate());
                buffer.putInt(audioStream.getChannelNum());
                buffer.putInt(audioStream.getBitsPerSample());
                buffer.putInt(0);//vps length
                buffer.putInt(mSps.length);
                buffer.putInt(mPps.length);
                buffer.putInt(0);
                buffer.put(mVps);
                buffer.put(mSps,0,mSps.length);
                if(mSps.length < 128) {
                    buffer.put(mVps, 0, 128 - mSps.length);
                }
                buffer.put(mPps,0,mPps.length);
                if(mPps.length < 36) {
                    buffer.put(mVps, 0, 36 - mPps.length);
                }
                buffer.put(mMei);
                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM:
                StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Info", "EASY_IPCAMERA_STATE_REQUEST_PLAY_STREAM"));
                startMediaCodec();
                audioStream.startRecord();

                break;
            case EasyIPCamera.ChannelState.EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM:
                StatusInfoView.getInstence().addInfoMsg(new StatusInfoView.StatusInfo("Info", "EASY_IPCAMERA_STATE_REQUEST_STOP_STREAM"));
                stopMediaCodec();
                audioStream.stop();
                break;
            default:
                break;
        }
    }
}
