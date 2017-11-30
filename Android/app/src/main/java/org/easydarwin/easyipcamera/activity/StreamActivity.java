/*
	Copyright (c) 2012-2017 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
package org.easydarwin.easyipcamera.activity;

import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import org.easydarwin.easyipcamera.R;
import org.easydarwin.easyipcamera.camera.EasyIPCamera;
import org.easydarwin.easyipcamera.camera.MediaStream;
import org.easydarwin.easyipcamera.updatemgr.UpdateMgr;
import org.easydarwin.easyipcamera.util.Util;
import org.easydarwin.easyipcamera.view.StatusInfoView;

import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("deprecation")
public class StreamActivity extends AppCompatActivity implements SurfaceHolder.Callback, View.OnClickListener {

    static final String TAG = "StreamActivity";
    public static final int REQUEST_MEDIA_PROJECTION = 1002;

    //默认分辨率
    int width = 640, height = 480;
    Button btnSwitch;
    Button btnSetting;
    Button btnPushScreen;
    TextView txtStreamAddress;
    Button btnSwitchCemera;
    Spinner spnResolution;
    List<String> listResolution;
    MediaStream mMediaStream;
    private StatusInfoView mDbgInfoPrint;
    static Intent mResultIntent;
    static int mResultCode;
    boolean mBackgroundCamera;
    SurfaceHolder mSurfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        spnResolution = (Spinner) findViewById(R.id.spn_resolution);

        mDbgInfoPrint = (StatusInfoView) findViewById(R.id.dbg_status_info);
        initDbgInfoView();

        btnSwitch = (Button) findViewById(R.id.btn_switch);
        btnSwitch.setOnClickListener(this);
        btnSetting = (Button) findViewById(R.id.btn_setting);
        btnSetting.setOnClickListener(this);
        btnSwitchCemera = (Button) findViewById(R.id.btn_switchCamera);
        btnSwitchCemera.setOnClickListener(this);
        btnPushScreen = (Button) findViewById(R.id.push_screen);
        btnPushScreen.setOnClickListener(this);
        txtStreamAddress = (TextView) findViewById(R.id.txt_stream_address);
        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.sv_surfaceview);
        surfaceView.getHolder().addCallback(this);
        surfaceView.getHolder().setFixedSize(getResources().getDisplayMetrics().widthPixels,
                getResources().getDisplayMetrics().heightPixels);
        surfaceView.setOnClickListener(this);

        listResolution = new ArrayList<String>();
        listResolution = Util.getSupportResolution(this);
        boolean supportdefault = listResolution.contains(String.format("%dx%d", width, height));
        if (!supportdefault) {
            String r = listResolution.get(0);
            String[] splitR = r.split("x");
            width = Integer.parseInt(splitR[0]);
            height = Integer.parseInt(splitR[1]);
        }

        if(EasyApplication.sMS == null){
            mMediaStream = new MediaStream(getApplicationContext(), null);
            EasyApplication.sMS = mMediaStream;
        } else {
            mMediaStream = EasyApplication.sMS;
            if(mMediaStream.isOpen()){
                btnSetting.setEnabled(false);
                btnPushScreen.setEnabled(false);
                btnSwitchCemera.setEnabled(false);

                String ip = Util.getLocalIpAddress();
                String port = EasyApplication.getEasyApplication().getPort();
                String id = EasyApplication.getEasyApplication().getId();
                btnSwitch.setText("停止");
                txtStreamAddress.setVisibility(View.VISIBLE);
                StatusInfoView.getInstence().setVisibility(View.VISIBLE);
                txtStreamAddress.setText(String.format(getRTSPAddr()));
            }
        }

        mBackgroundCamera = PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getBoolean("background-camera", false);
//        if(mBackgroundCamera) {
//            Intent bgcamera = new Intent(StreamActivity.this, BackgroundCameraService.class);
//            stopService(bgcamera);
//            startService(bgcamera);
//        } else {
//            mMediaStream.setSurfaceHolder(mSurfaceHolder);
//            mMediaStream.createCamera();
//            mMediaStream.startPreview();
//        }
        mMediaStream.updateResolution(width, height);
        mMediaStream.setDgree(getDgree());
        initSpninner();

        if (RecordService.mEasyIPCamera != null){
            btnPushScreen.setText("停止推送屏幕");
            TextView viewById = (TextView) findViewById(R.id.txt_stream_address);
            viewById.setText(getRTSPAddr());
        }

        UpdateMgr update = new UpdateMgr(this);
        update.checkUpdate();
    }

    private void initDbgInfoView(){
        if(mDbgInfoPrint == null)
            return;
        ViewGroup.LayoutParams lp = mDbgInfoPrint.getLayoutParams();
        lp.height = getResources().getDisplayMetrics().heightPixels/4;
        mDbgInfoPrint.setLayoutParams(lp);
        mDbgInfoPrint.requestLayout();
        mDbgInfoPrint.setInstence(mDbgInfoPrint);
    }


    private void initSpninner() {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, R.layout.spn_item, listResolution);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spnResolution.setAdapter(adapter);
        int position = listResolution.indexOf(String.format("%dx%d", width, height));
        spnResolution.setSelection(position,false);
        spnResolution.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String r = listResolution.get(position);
                String[] splitR = r.split("x");
                width = Integer.parseInt(splitR[0]);
                height = Integer.parseInt(splitR[1]);
                mMediaStream.updateResolution(width, height);
                mMediaStream.reStartStream();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });
    }

    @Override
    public void surfaceCreated(final SurfaceHolder holder) {
        mBackgroundCamera = PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getBoolean("background-camera", false);
        mSurfaceHolder = holder;
        if(mBackgroundCamera){
            if(!mMediaStream.isOpen()) {
                Intent bgcamera = new Intent(StreamActivity.this, BackgroundCameraService.class);
                stopService(bgcamera);
                startService(bgcamera);
            }
        } else {
            mMediaStream.setSurfaceHolder(holder);
            mMediaStream.createCamera();
            mMediaStream.startPreview();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (!mBackgroundCamera) {
            mMediaStream.stopPreview();
            mMediaStream.destroyCamera();
            if (mMediaStream.isOpen()) {
                mMediaStream.stopChannel();
            }
        }
    }


    private int getDgree() {
        int rotation = getWindowManager().getDefaultDisplay().getRotation();
        int degrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0:
                degrees = 0;
                break; // Natural orientation
            case Surface.ROTATION_90:
                degrees = 90;
                break; // Landscape left
            case Surface.ROTATION_180:
                degrees = 180;
                break;// Upside down
            case Surface.ROTATION_270:
                degrees = 270;
                break;// Landscape right
        }
        return degrees;
    }

    private String getRTSPAddr(){
        String ip = Util.getLocalIpAddress();
        String port = EasyApplication.getEasyApplication().getPort();
        String id = EasyApplication.getEasyApplication().getId();
        return String.format("rtsp://%s:%s/%s", ip, port, id);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_switch:
                StatusInfoView.getInstence().clearMsg();
                if (!mMediaStream.isOpen()) {
                    btnSetting.setEnabled(false);
                    btnPushScreen.setEnabled(false);
                    btnSwitchCemera.setEnabled(false);

                    String ip = Util.getLocalIpAddress();
                    String port = EasyApplication.getEasyApplication().getPort();
                    String id = EasyApplication.getEasyApplication().getId();
                    mMediaStream.startChannel(ip, port, id);
                    btnSwitch.setText("停止");
                    txtStreamAddress.setVisibility(View.VISIBLE);
                    StatusInfoView.getInstence().setVisibility(View.VISIBLE);
                    txtStreamAddress.setText(getRTSPAddr());
                } else {
                    txtStreamAddress.setVisibility(View.INVISIBLE);
                    StatusInfoView.getInstence().setVisibility(View.INVISIBLE);
                    mMediaStream.stopChannel();
                    btnSwitch.setText("推送摄像头");

                    btnSetting.setEnabled(true);
                    btnPushScreen.setEnabled(true);
                    btnSwitchCemera.setEnabled(true);
                }
                break;
            case R.id.btn_setting:
                startActivity(new Intent(this, SettingActivity.class));
                break;
            case R.id.sv_surfaceview:
                try {
                    mMediaStream.getCamera().autoFocus(null);
                } catch (Exception e) {
                }
                break;
            case R.id.btn_switchCamera: {
                mMediaStream.setDgree(getDgree());
                mMediaStream.switchCamera();
                break;
            }
            case R.id.push_screen:
                if (!mMediaStream.isOpen()) {
                    spnResolution.setEnabled(true);
                }
                //sendMessage("");
                boolean SWcodec = PreferenceManager.getDefaultSharedPreferences(EasyApplication.getEasyApplication()).getBoolean("key-sw-codec", false);
                if(SWcodec){
                    new AlertDialog.Builder(this).setMessage("推送屏幕暂时只支持硬编码，请使用硬件编码。").setTitle("抱歉").show();
                } else {
                    onPushScreen();
                }
                break;
            default:
                break;
        }
    }

    private void startScreenPushIntent() {
        if (StreamActivity.mResultIntent != null && StreamActivity.mResultCode != 0) {
            RecordService.mEasyIPCamera = new EasyIPCamera();
            RecordService.mEasyIPCamera.active(getApplicationContext());
            Intent intent = new Intent(getApplicationContext(), RecordService.class);
            startService(intent);

            txtStreamAddress.setVisibility(View.VISIBLE);
            txtStreamAddress.setText(String.format(getRTSPAddr()));

            btnPushScreen.setText("停止推送屏幕");
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {

                MediaProjectionManager mMpMngr = (MediaProjectionManager) getApplicationContext().getSystemService(MEDIA_PROJECTION_SERVICE);
                startActivityForResult(mMpMngr.createScreenCaptureIntent(), StreamActivity.REQUEST_MEDIA_PROJECTION);

            }
        }
    }

    public void onPushScreen() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP){
            new AlertDialog.Builder(this).setMessage("推送屏幕需要安卓5.0以上,您当前系统版本过低,不支持该功能。").setTitle("抱歉").show();
            return;
        }

        if (!PreferenceManager.getDefaultSharedPreferences(this).getBoolean("alert_screen_background_pushing", false)){
            new AlertDialog.Builder(this).setTitle("提醒").setMessage("屏幕直播将要开始,直播过程中您可以切换到其它屏幕。不过记得直播结束后,再进来停止直播哦!").setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialogInterface, int i) {
                    PreferenceManager.getDefaultSharedPreferences(StreamActivity.this).edit().putBoolean("alert_screen_background_pushing", true).apply();
                    onPushScreen();
                }
            }).show();
            return;
        }

        if (RecordService.mEasyIPCamera != null) {
            Intent intent = new Intent(getApplicationContext(), RecordService.class);
            stopService(intent);

            TextView viewById = (TextView) findViewById(R.id.txt_stream_address);
            viewById.setText(null);
            btnPushScreen.setText("推送屏幕");
            btnSwitch.setEnabled(true);
            btnSwitchCemera.setEnabled(true);
            btnSetting.setEnabled(true);
        }else{
            btnSwitch.setEnabled(false);
            btnSwitchCemera.setEnabled(false);
            btnSetting.setEnabled(false);
            startScreenPushIntent();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_MEDIA_PROJECTION) {
            if (resultCode == RESULT_OK) {
                Log.e(TAG,"get capture permission success!");
                mResultCode = resultCode;
                mResultIntent = data;

                startScreenPushIntent();

            }
        }
    }

    @Override
    protected void onDestroy() {
        mMediaStream.destroyChannel();
        StatusInfoView.getInstence().uninit();
        super.onDestroy();
    }

    @Override
    protected void onStop() {
        StatusInfoView.getInstence().uninit();
        super.onStop();
    }

    /**
     * Take care of popping the fragment back stack or finishing the activity
     * as appropriate.
     */
    @Override
    public void onBackPressed() {
        boolean isStreaming = mMediaStream != null && mMediaStream.isOpen();
        if (isStreaming && PreferenceManager.getDefaultSharedPreferences(this).getBoolean("key_enable_background_camera", true)){
            if (!PreferenceManager.getDefaultSharedPreferences(this).getBoolean("background_camera_alert", false)){
                new AlertDialog.Builder(this).setTitle("提醒").setMessage("您设置了使能摄像头后台采集,因此摄像头将会继续在后台采集并上传视频。记得直播结束后,再回来这里关闭直播。").setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        PreferenceManager.getDefaultSharedPreferences(StreamActivity.this).edit().putBoolean("background_camera_alert", true).apply();
                        StreamActivity.super.onBackPressed();
                    }
                }).show();
                return;
            }
            super.onBackPressed();
        }else{
            super.onBackPressed();
        }
    }
}
