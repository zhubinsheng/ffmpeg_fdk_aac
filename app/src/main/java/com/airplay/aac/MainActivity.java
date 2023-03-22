package com.airplay.aac;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.airplay.aac.databinding.ActivityMainBinding;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'aac' library on application startup.
    static {
        System.loadLibrary("faac");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // Example of a call to a native method
        TextView tv = binding.sampleText;

        new Thread(new Runnable() {
            @Override
            public void run() {
                initDecoder();
            }
        }).start();

        new Thread(new Runnable() {
            @Override
            public void run() {
                InputStream i = getAssetsStream("test.aac");
                try {
                    inputStreamToByteArray(i);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }

    /**
     * inputStream转byte数组
     *
     * @param inputStream 输入流对象
     * @return byte数组
     */
    public void inputStreamToByteArray(InputStream inputStream) throws IOException {
        byte[] buffer = new byte[1024];
        int num;
        while ((num = inputStream.read(buffer)) != -1) {
            Log.d("faac", "read: " + buffer.length);
            addPacket(buffer);
        }
    }


    /**
     * 得到Assets里面相应的文件流
     *
     * @param fileName
     * @return
     */
    private InputStream getAssetsStream(String fileName) {
        InputStream is = null;
        try {
            is = getAssets().open(fileName);
            //is.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return is;
    }

    /**
     * A native method that is implemented by the 'aac' native library,
     * which is packaged with this application.
     */
    public native String initDecoder();

    public native void addPacket(byte[] buffer);

}