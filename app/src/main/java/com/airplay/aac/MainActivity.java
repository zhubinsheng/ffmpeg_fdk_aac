package com.airplay.aac;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.TextView;

import com.airplay.aac.databinding.ActivityMainBinding;

import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends Activity {

    // Used to load the 'aac' library on application startup.
    static {
        System.loadLibrary("fdkdec");
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

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                initFdk();


                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            Thread.sleep(300);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }

                        for (int i = 1; i < 654; i++) {
                            InputStream inputStream = getAssetsStream("audio/dump"+i+".aac");
                            try {
                                byte[] buffer = new byte[4096 * 4];
                                int num;
                                while ((num = inputStream.read(buffer)) != -1) {
                                    Log.d("faac", "read: " + num);
                                    decodeFdk(buffer, num);
                                    Thread.sleep(10);
                                }
                            } catch (IOException e) {
                                e.printStackTrace();
                            } catch (InterruptedException e) {
                                throw new RuntimeException(e);
                            }
                        }
                    }
                }).start();

            }
        }, 1000);

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                initDecoder();
            }
        });

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(300);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }

                InputStream i = getAssetsStream("test.aac");
                try {
                    inputStreamToByteArray(i);
                } catch (IOException e) {
                    e.printStackTrace();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        });
    }

    /**
     * inputStream转byte数组
     *
     * @param inputStream 输入流对象
     * @return byte数组
     */
    public void inputStreamToByteArray(InputStream inputStream) throws IOException, InterruptedException {
        byte[] buffer = new byte[20480 * 10];
        int num;
        while ((num = inputStream.read(buffer)) != -1) {
            Log.d("faac", "read: " + buffer.length);
            addPacket(buffer);
            Thread.sleep(10);
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
    public native void initDecoder();

    public native void addPacket(byte[] buffer);

    public native void initFdk();

    public native void decodeFdk(byte[] buffer, int num);

}