package com.example.nexusmouse

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.io.OutputStream
import java.net.Socket

class MainActivity : AppCompatActivity(), SensorEventListener {

    private lateinit var sensorManager: SensorManager
    private var accelerometer: Sensor? = null
    private var gyroscope: Sensor? = null

    private lateinit var tvAccelValues: TextView
    private lateinit var tvGyroValues: TextView
    private lateinit var tvConnStatus: TextView
    private lateinit var btnConnectUsb: Button
    private lateinit var btnToggleStream: Button

    private var socket: Socket? = null
    private var outputStream: OutputStream? = null

    private var isSending = false
    private var lastSendTime: Long = 0

    private var accelX = 0f; private var accelY = 0f; private var accelZ = 0f
    private var gyroX = 0f; private var gyroY = 0f; private var gyroZ = 0f

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvAccelValues = findViewById(R.id.tvAccelValues)
        tvGyroValues = findViewById(R.id.tvGyroValues)
        tvConnStatus = findViewById(R.id.tvConnStatus)
        btnConnectUsb = findViewById(R.id.btnConnectUsb)
        btnToggleStream = findViewById(R.id.btnToggleStream)

        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)

        btnConnectUsb.setOnClickListener {
            connectToPcOverUsb()
        }

        btnToggleStream.setOnClickListener {
            if (outputStream == null) {
                Toast.makeText(this, "Connect USB first!", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            isSending = !isSending
            btnToggleStream.text = if (isSending) "Stop Sending" else "Start Sending"
        }
    }

    private fun connectToPcOverUsb() {
        tvConnStatus.text = "Status: Connecting..."

        Thread {
            try {
                // Close old socket if it exists
                socket?.close()

                // Connect using loopback bound by adb reverse
                socket = Socket("127.0.0.1", 12345)
                socket?.keepAlive = true
                outputStream = socket?.outputStream

                runOnUiThread {
                    tvConnStatus.text = "Status: Connected via USB"
                    Toast.makeText(this, "Connected to PC!", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                e.printStackTrace()
                runOnUiThread {
                    tvConnStatus.text = "Status: Failed (${e.localizedMessage})"
                    Toast.makeText(this, "Error: ${e.message}", Toast.LENGTH_LONG).show()
                }
            }
        }.start()
    }

    override fun onResume() {
        super.onResume()
        accelerometer?.let { sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_UI) }
        gyroscope?.let { sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_UI) }
    }

    override fun onPause() {
        super.onPause()
        sensorManager.unregisterListener(this)
    }

    override fun onSensorChanged(event: SensorEvent?) {
        if (event == null) return

        when (event.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> {
                accelX = event.values[0]; accelY = event.values[1]; accelZ = event.values[2]
                tvAccelValues.text = String.format("Accel: X: %.2f | Y: %.2f | Z: %.2f", accelX, accelY, accelZ)
            }
            Sensor.TYPE_GYROSCOPE -> {
                gyroX = event.values[0]; gyroY = event.values[1]; gyroZ = event.values[2]
                tvGyroValues.text = String.format("Gyro: X: %.2f | Y: %.2f | Z: %.2f", gyroX, gyroY, gyroZ)
            }
        }

        // Stream data down USB cable if stream is toggled ON
        val currentTime = System.currentTimeMillis()
        if (isSending && outputStream != null && (currentTime - lastSendTime > 30)) {
            lastSendTime = currentTime
            val dataPacket = String.format("A:%.2f,%.2f,%.2f|G:%.2f,%.2f,%.2f\n", accelX, accelY, accelZ, gyroX, gyroY, gyroZ)

            Thread {
                try {
                    outputStream?.write(dataPacket.toByteArray())
                    outputStream?.flush()
                } catch (e: Exception) {
                    isSending = false
                    runOnUiThread {
                        btnToggleStream.text = "Start Sending"
                        tvConnStatus.text = "Status: Disconnected"
                    }
                }
            }.start()
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onDestroy() {
        super.onDestroy()
        try {
            socket?.close()
        } catch (_: Exception) {}
    }
}