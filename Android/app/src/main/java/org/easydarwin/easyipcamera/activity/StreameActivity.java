/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
package org.easydarwin.easyipcamera.activity;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import org.easydarwin.easyipcamera.R;
import org.easydarwin.easyipcamera.camera.MediaStream;
import org.easydarwin.easyipcamera.util.Util;

import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("deprecation")
public class StreameActivity extends AppCompatActivity implements SurfaceHolder.Callback, View.OnClickListener {

    static final String TAG = "StreameActivity";

    //默认分辨率
    int width = 640, height = 480;
    Button btnSwitch;
    Button btnSetting;
    TextView txtStreamAddress;
    Button btnSwitchCemera;
    Spinner spnResolution;
    List<String> listResolution;
    MediaStream mMediaStream;
    TextView txtStatus;
    private boolean mIsStarted = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        spnResolution = (Spinner) findViewById(R.id.spn_resolution);

        txtStatus = (TextView) findViewById(R.id.txt_stream_status);
        btnSwitch = (Button) findViewById(R.id.btn_switch);
        btnSwitch.setOnClickListener(this);
        btnSetting = (Button) findViewById(R.id.btn_setting);
        btnSetting.setOnClickListener(this);
        btnSwitchCemera = (Button) findViewById(R.id.btn_switchCamera);
        btnSwitchCemera.setOnClickListener(this);
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

        mMediaStream = new MediaStream(getApplicationContext(), surfaceView);
        mMediaStream.updateResolution(width, height);
        mMediaStream.setDgree(getDgree());
        initSpninner();

        mIsStarted = false;
    }

    private static final String STATE = "state";
    private static final int MSG_STATE = 1;

    private void sendMessage(String message) {
        Message msg = Message.obtain();
        msg.what = MSG_STATE;
        Bundle bundle = new Bundle();
        bundle.putString(STATE, message);
        msg.setData(bundle);
        handler.sendMessage(msg);
    }

    Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_STATE:
                    String state = msg.getData().getString("state");
                    txtStatus.setText(state);
                    break;
            }
        }
    };

    private void initSpninner() {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, R.layout.spn_item, listResolution);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spnResolution.setAdapter(adapter);
        int position = listResolution.indexOf(String.format("%dx%d", width, height));
        spnResolution.setSelection(position);
        spnResolution.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String r = listResolution.get(position);
                String[] splitR = r.split("x");
                width = Integer.parseInt(splitR[0]);
                height = Integer.parseInt(splitR[1]);
                mMediaStream.updateResolution(width, height);
                //mMediaStream.reStartStream();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mMediaStream.createCamera();
        mMediaStream.startPreview();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mMediaStream.stopPreview();
        mMediaStream.stopStream();
        mMediaStream.destroyCamera();
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

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_switch:
                if (!mIsStarted) {
                    String ip = Util.getLocalIpAddress();
                    String port = EasyApplication.getEasyApplication().getPort();
                    String id = EasyApplication.getEasyApplication().getId();
                    mMediaStream.startStream(ip, port, id);
                    btnSwitch.setText("停止");
                    txtStreamAddress.setVisibility(View.VISIBLE);
                    txtStreamAddress.setText(String.format("rtsp://%s:%s/%s", ip, port, id));
                    mIsStarted = true;
                } else {
                    txtStreamAddress.setVisibility(View.INVISIBLE);
                    mMediaStream.stopStream();
                    btnSwitch.setText("开始");
                    mIsStarted = false;
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
            }
            break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
        mMediaStream.destroyStream();
    }
}
