package ec.edu.ups.interciclovisioncomputador;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;

import com.longdo.mjpegviewer.MjpegView;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("interciclovisioncomputador");
    }

    private Boolean isProcessing;
    private ImageView imageView;
    private MjpegView mjpegViewer;
    private Bitmap capturedBitmap;
    private Bitmap resultBitmap;
    private Canvas canvas;
    private SeekBar seekBarStepSize, seekBarThreshold1, seekBarThreshold2;
    private TextView textViewStepSizeValue, textViewThreshold1Value, textViewThreshold2Value;
    private int stepSize = 4;  // Valor inicial por defecto
    private int threshold1 = 100;
    private int threshold2 = 100;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initializeUI();
        setupMjpegStream();
        startFrameProcessing();
    }

    private void initializeUI() {
        imageView = findViewById(R.id.image_view);
        mjpegViewer = findViewById(R.id.mjpegview);
        seekBarStepSize = findViewById(R.id.seekBarStepSize);
        seekBarThreshold1 = findViewById(R.id.seekBarThreshold1);
        seekBarThreshold2 = findViewById(R.id.seekBarThreshold2);

        textViewStepSizeValue = findViewById(R.id.seekBarLabelStepSize);
        textViewThreshold1Value = findViewById(R.id.seekBarLabelThreshold1);
        textViewThreshold2Value = findViewById(R.id.seekBarLabelThreshold2);

        configureUI(); // Llama a la configuración de SeekBars
    }

    private void configureUI() {
        textViewStepSizeValue.setText(String.valueOf(stepSize));
        seekBarStepSize.setMax(6);
        seekBarStepSize.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                stepSize = 4 + progress;  // Rango 5-10
                textViewStepSizeValue.setText(String.valueOf(stepSize));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        textViewThreshold1Value.setText(String.valueOf(threshold1));
        seekBarThreshold1.setMax(500);
        seekBarThreshold1.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                threshold1 = 100 + progress;
                textViewThreshold1Value.setText(String.valueOf(threshold1));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        textViewThreshold2Value.setText(String.valueOf(threshold2));
        seekBarThreshold2.setMax(500);
        seekBarThreshold2.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                threshold2 = 100 + progress;
                textViewThreshold2Value.setText(String.valueOf(threshold2));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void setupMjpegStream() {
        if (mjpegViewer != null) {
            mjpegViewer.setMode(MjpegView.MODE_FIT_WIDTH);
            mjpegViewer.setAdjustHeight(true);
            mjpegViewer.setSupportPinchZoomAndPan(true);
            mjpegViewer.setUrl("http://pendelcam.kip.uni-heidelberg.de/mjpg/video.mjpg");
            mjpegViewer.startStream();
        } else {
            Log.e("MainActivity", "MjpegView is not initialized. Check your layout file.");
        }
    }

    private void stopMjpegStream() {
        if (mjpegViewer != null) {
            mjpegViewer.stopStream();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopMjpegStream();
    }

    private void startFrameProcessing() {
        isProcessing = true;
        Handler frameHandler = new Handler();
        frameHandler.post(new Runnable() {
            @Override
            public void run() {
                if (!isProcessing) return;

                int width = mjpegViewer.getWidth();
                int height = mjpegViewer.getHeight();

                if (width > 0 && height > 0) {
                    if (capturedBitmap == null || capturedBitmap.getWidth() != width || capturedBitmap.getHeight() != height) {
                        capturedBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                        resultBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                        canvas = new Canvas(capturedBitmap);
                    }
                    mjpegViewer.draw(canvas);
                    procesarFrameEnC(capturedBitmap, resultBitmap, stepSize, threshold1, threshold2);
                    imageView.setImageBitmap(resultBitmap);
                }
                frameHandler.postDelayed(this, 33);  // 33 ms ≈ 30 FPS
            }
        });
    }

    public native void procesarFrameEnC(Bitmap bitmapIn, Bitmap bitmapOut, int stepSize, int threshold1, int threshold2);
}