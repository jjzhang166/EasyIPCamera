package org.easydarwin.easyipcamera.sw;

public class X264Encoder {

    static {
        System.loadLibrary("x264enc");
    }

    private long mHandle;

    /**
     * 创建编码器
     *
     * @param w       要编码的视频的宽度
     * @param h       要编码的视频的高度
     * @param bitrate 要编码的码率
     */
    public void create(int w, int h, int frameRate, int bitrate) {
        long[] handle = new long[1];
        create(w, h, frameRate, bitrate, handle);
        mHandle = handle[0];
    }

    /**
     * 获取头信息
     *
     * @param sps      yv12格式的视频数据（数据长度应该为w*h*1.5）
     * @param spslen    视频数据的偏移（即在yv12里的起始位）
     * @param pps       编码后的数据。
     * @param ppslen 编码后的视频数据的偏移（即在out里的起始位）
     * @return returns negative on error, zero if success.
     */
    public int getSpsPps(byte[] sps, int[] spslen, byte[] pps, int[] ppslen) {
        return getSpsPps(mHandle, sps, spslen, pps, ppslen);
    }

    /**
     * 编码
     *
     * @param yv12      yv12格式的视频数据（数据长度应该为w*h*1.5）
     * @param offset    视频数据的偏移（即在yv12里的起始位）
     * @param out       编码后的数据。
     * @param outOffset 编码后的视频数据的偏移（即在out里的起始位）
     * @param outLen    outLen[0]为编码后的视频数据的长度
     * @param keyFrame  keyFrame[0]为编码后的视频帧的关键帧标识
     * @return returns negative on error, zero if no NAL units returned.
     */
    public int encode(byte[] yv12, int offset, byte[] out, int outOffset, int[] outLen, byte[] keyFrame) {
        return encode(mHandle, yv12, offset, out, outOffset, outLen, keyFrame);
    }

    /**
     * 关闭编码器
     */
    public void close() {
        close(mHandle);
    }

    private static native void create(int width, int height, int frameRate, int bitRate, long[] handle);

    private static native int getSpsPps(long handle, byte[] sps, int[] spslen, byte[] pps, int[] ppslen);

    private static native int encode(long handle, byte[] buffer, int offset, byte[] out, int outOffset, int[] outLen, byte[] keyFrame);

    private static native void close(long handle);
}
