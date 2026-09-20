#include <stdio.h>
#include <math.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

// Filter Parameters
#define ALPHA_ACCEL 0.15f
#define ALPHA_GYRO  0.80f

// Deadzone Thresholds
#define DZ_ACCEL_X 0.03f
#define DZ_ACCEL_Y 0.03f
#define DZ_ACCEL_Z 0.00f

#define DZ_GYRO_X  0.00f
#define DZ_GYRO_Y  0.01f
#define DZ_GYRO_Z  0.00f

// Sensitivity Parameters
#define SENSITIVITY_DESK_ACCEL 15.0f
#define SENSITIVITY_DESK_GYRO  20.0f
#define SENSITIVITY_AIR        25.0f

static float prev_accelX = 0.0f, prev_accelY = 0.0f, prev_accelZ = 0.0f;
static float prev_gyroX  = 0.0f, prev_gyroY  = 0.0f, prev_gyroZ  = 0.0f;

float apply_ema(float current_val, float prev_val, float alpha) {
    return (alpha * current_val) + ((1.0f - alpha) * prev_val);
}

float apply_deadzone(float value, float threshold) {
    if (threshold > 0.0f && fabsf(value) < threshold) {
        return 0.0f;
    }
    return value;
}

void process_sensor_data(float raw_ax, float raw_ay, float raw_az,
                         float raw_gx, float raw_gy, float raw_gz,
                         float *out_ax, float *out_ay, float *out_az,
                         float *out_gx, float *out_gy, float *out_gz) 
{
    // Low-Pass Filtering
    prev_accelX = apply_ema(raw_ax, prev_accelX, ALPHA_ACCEL);
    prev_accelY = apply_ema(raw_ay, prev_accelY, ALPHA_ACCEL);
    prev_accelZ = apply_ema(raw_az, prev_accelZ, ALPHA_ACCEL);

    *out_ax = apply_deadzone(prev_accelX, DZ_ACCEL_X);
    *out_ay = apply_deadzone(prev_accelY, DZ_ACCEL_Y);
    *out_az = apply_deadzone(prev_accelZ, DZ_ACCEL_Z);

    prev_gyroX = apply_ema(raw_gx, prev_gyroX, ALPHA_GYRO);
    prev_gyroY = apply_ema(raw_gy, prev_gyroY, ALPHA_GYRO);
    prev_gyroZ = apply_ema(raw_gz, prev_gyroZ, ALPHA_GYRO);

    *out_gx = apply_deadzone(prev_gyroX, DZ_GYRO_X);
    *out_gy = apply_deadzone(prev_gyroY, DZ_GYRO_Y);
    *out_gz = apply_deadzone(prev_gyroZ, DZ_GYRO_Z);
}

void move_mouse(int dx, int dy) {
    if (dx == 0 && dy == 0) return;

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));
}

void clearConsoleLine() {
    COORD coord = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[256] = {0};

    WSAStartup(MAKEWORD(2,2), &wsa);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(12345);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    system("cls");
    printf("Waiting for USB connection from phone on port 12345...\n");

    new_socket = accept(server_fd, (struct sockaddr *)&address, (int*)&addrlen);
    
    system("cls");
    printf("--- USB Sensor Stream Connected! ---\n\n");

    int valread;
    while ((valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[valread] = '\0';
        float raw_ax, raw_ay, raw_az, raw_gx, raw_gy, raw_gz;
        float clean_ax, clean_ay, clean_az, clean_gx, clean_gy, clean_gz;
        
        if (sscanf(buffer, "A:%f,%f,%f|G:%f,%f,%f", &raw_ax, &raw_ay, &raw_az, &raw_gx, &raw_gy, &raw_gz) == 6) {
            process_sensor_data(raw_ax, raw_ay, raw_az, raw_gx, raw_gy, raw_gz,
                                &clean_ax, &clean_ay, &clean_az, &clean_gx, &clean_gy, &clean_gz);
            
            clearConsoleLine();
            printf("--- USB Sensor Stream Connected! ---\n");
            
            int dx = 0;
            int dy = 0;

            // Threshold: clean_az > 7.5 m/s^2 indicates phone is lying flat
            if (clean_az > 7.5f) {
                // --- MODE 1: DESK MOUSE MODE ---
                // Linear translation component
                float linear_x = clean_ax * SENSITIVITY_DESK_ACCEL;
                float linear_y = -clean_ay * SENSITIVITY_DESK_ACCEL;

                // Angular pivot component
                float hyp = sqrtf((clean_ax * clean_ax) + (clean_ay * clean_ay));
                float pivot_x = 0.0f;
                float pivot_y = 0.0f;

                if (hyp > 0.02f) {
                    float cos_theta = clean_ax / hyp;
                    float sin_theta = clean_ay / hyp;
                    pivot_x = clean_gz * cos_theta * SENSITIVITY_DESK_GYRO;
                    pivot_y = -clean_gz * sin_theta * SENSITIVITY_DESK_GYRO;
                } else {
                    pivot_x = clean_gz * SENSITIVITY_DESK_GYRO;
                }

                dx = (int)(linear_x + pivot_x);
                dy = (int)(linear_y + pivot_y);

                printf("MODE: DESK | dX: %3d | dY: %3d | Az: %.2f\n", dx, dy, clean_az);

            } else {
                // --- MODE 2: AIR MOUSE MODE ---
                // Direct mapping: Yaw (Gyro Z) -> Screen X, Pitch (Gyro X) -> Screen Y
                dx = (int)(-clean_gz * SENSITIVITY_AIR);
                dy = (int)(-clean_gx * SENSITIVITY_AIR);

                printf("MODE: AIR  | dX: %3d | dY: %3d | Az: %.2f\n", dx, dy, clean_az);
            }

            // Send movement command to OS
            move_mouse(dx, dy);
            
            printf("Accel [X: %5.2f | Y: %5.2f | Z: %5.2f]\n", clean_ax, clean_ay, clean_az);
            printf("Gyro  [X: %5.2f | Y: %5.2f | Z: %5.2f]\n", clean_gx, clean_gy, clean_gz);
        }
    }

    closesocket(new_socket);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}