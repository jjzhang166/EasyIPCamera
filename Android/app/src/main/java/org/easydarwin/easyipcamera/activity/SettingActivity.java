/*
	Copyright (c) 2012-2017 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/

package org.easydarwin.easyipcamera.activity;

import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.easydarwin.easyipcamera.R;
import org.easydarwin.easyipcamera.camera.MediaStream;
import org.easydarwin.easyipcamera.config.Config;
import org.easydarwin.easyipcamera.util.Util;

public class SettingActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);
        final TextView txtIp = (TextView) findViewById(R.id.edt_server_address);
        final EditText txtPort = (EditText) findViewById(R.id.edt_server_port);
        final EditText txtId = (EditText) findViewById(R.id.edt_stream_id);

        String ip = Util.getLocalIpAddress();
        String port = EasyApplication.getEasyApplication().getPort();
        String id = EasyApplication.getEasyApplication().getId();

        txtIp.setText("本地IP : "+ip);
        txtPort.setText(port);
        txtId.setText(id);

        CheckBox x264enc = (CheckBox) findViewById(R.id.use_x264_encode);
        x264enc.setChecked(PreferenceManager.getDefaultSharedPreferences(this).getBoolean("key-sw-codec", MediaStream.useSWCodec()));
        x264enc.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                PreferenceManager.getDefaultSharedPreferences(SettingActivity.this).edit().putBoolean("key-sw-codec", isChecked).apply();
            }
        });

        Button btnSave = (Button) findViewById(R.id.btn_save);
        btnSave.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String portValue = txtPort.getText().toString();
                String idValue = txtId.getText().toString();

                if (TextUtils.isEmpty(portValue)) {
                    portValue = Config.DEFAULT_SERVER_PORT;
                }

                if (TextUtils.isEmpty(idValue)) {
                    idValue = Config.DEFAULT_STREAM_ID;
                }

                EasyApplication.getEasyApplication().saveStringIntoPref(Config.SERVER_PORT, portValue);
                EasyApplication.getEasyApplication().saveStringIntoPref(Config.STREAM_ID, idValue);

                onBackPressed();
            }
        });
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }
}
