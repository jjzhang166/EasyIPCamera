package org.easydarwin.easyipcamera.updatemgr;

import com.google.gson.Gson;

import org.easydarwin.okhttplibrary.callback.Callback;

import okhttp3.Response;

/**
 * Created by Kim on 2016/8/25.
 */
public abstract class VersionCallback extends Callback<RemoteVersionInfo> {
    @Override
    public RemoteVersionInfo parseNetworkResponse(Response response) throws Exception {
        String string = response.body().string();
        RemoteVersionInfo versionInfo = new Gson().fromJson(string, RemoteVersionInfo.class);
        return versionInfo;
    }
}
