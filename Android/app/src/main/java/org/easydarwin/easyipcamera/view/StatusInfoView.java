package org.easydarwin.easyipcamera.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import java.util.ArrayList;

/**
 * Created by kim on 2016/8/18.
 */
public class StatusInfoView extends View {
    private Paint mPaint;
    private Context mContext;
    private static ArrayList<StatusInfo> mInfoList = null;
    private static final String TAG = "StatusInfoView";
    private static StatusInfoView mInstence;

    public StatusInfoView(Context context) {
        super(context);
        mContext = context;
        mInfoList = new ArrayList<StatusInfo>();
        mInfoList.clear();
    }
    public StatusInfoView(Context context, AttributeSet attr) {
        super(context,attr);
        mContext = context;
        mInfoList = new ArrayList<StatusInfo>();
        mInfoList.clear();
    }

    public static StatusInfoView getInstence(){
        return mInstence;
    }

    public void setInstence(StatusInfoView instence){
        mInstence = instence;
        handler.postDelayed(runnable, 1000);
    }

    public void addInfoMsg(StatusInfo info){
        mInfoList.add(info);
    }

    public void clearMsg(){
        mInfoList.clear();
    }

    Handler handler=new Handler();
    Runnable runnable=new Runnable(){
        @Override
        public void run() {
            invalidate();
            handler.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        int i = 0;
        mPaint = new Paint();
        mPaint.setTextSize(25);

        ViewGroup.LayoutParams lp = this.getLayoutParams();
        int posY = this.getHeight() - 20;
        for(i = mInfoList.size()-1; i >= 0; i--){
            if(posY < 0) {
                mInfoList.remove(i);
                continue;
            }

            StatusInfo info = mInfoList.get(i);
            if(info.level.equals("Info")){
                mPaint.setColor(Color.WHITE);
            } else if(info.level.equals("Warn")){
                mPaint.setColor(Color.YELLOW);
            }
            canvas.drawText(info.level + ": "+ info.values, 5, posY , mPaint);
            posY -= 35;
        }
    }

    public static class StatusInfo{
        String level;
        String values;

        public StatusInfo(String level, String values){
            this.level = level;
            this.values = values;
        }
    }
}
