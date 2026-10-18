#include <stdio.h>
#include <math.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define ALPHA_ACCEL 0.25f
#define ALPHA_GYRO  0.70f

#define DZ_ACCEL_X  0.05f
#define DZ_ACCEL_Y  0.05f
#define DZ_GYRO_Z   0.04f

#define SENSITIVITY_X     12.0f
#define SENSITIVITY_Y     14.0f
#define SENSITIVITY_GYRO  16.0f
#define FRICTION_DAMPING  0.75f  // Friction decay to stop smooth glide instantly

static float bias_ax = 0.0f, bias_ay = 0.0f, bias_gz = 0.0f;
static int cal_samples = 0;
#define CALIBRATION_TICKS 100

static float prev_ax = 0.0f, prev_ay = 0.0f, prev_az = 9.81f;
static float prev_gz = 0.0f;

// Motion state velocity registers
static float vel_x = 0.0f, vel_y = 0.0f;
static float last_ax_dir = 0.0f, last_ay_dir = 0.0f;

float apply_ema(float curr, float prev, float alpha) {
    return (alpha * curr) + ((1.0f - alpha) * prev);
}

float apply_deadzone(float val, float dz) {
    return (fabsf(val) < dz) ? 0.0f : val;
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
    printf("Waiting for USB connection on port 12345...\n");

    new_socket = accept(server_fd, (struct sockaddr *)&address, (int*)&addrlen);
    
    system("cls");
    printf("--- CONNECTED! KEEP PHONE STILL FOR 2 SECONDS ---\n");

    int valread;
    while ((valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[valread] = '\0';
        float raw_ax, raw_ay, raw_az, raw_gx, raw_gy, raw_gz;
        
        if (sscanf(buffer, "A:%f,%f,%f|G:%f,%f,%f", &raw_ax, &raw_ay, &raw_az, &raw_gx, &raw_gy, &raw_gz) == 6) {
            
            // 1. Zero-Bias Auto Calibration
            if (cal_samples < CALIBRATION_TICKS) {
                bias_ax += raw_ax;
                bias_ay += raw_ay;
                bias_gz += raw_gz;
                cal_samples++;
                if (cal_samples == CALIBRATION_TICKS) {
                    bias_ax /= CALIBRATION_TICKS;
                    bias_ay /= CALIBRATION_TICKS;
                    bias_gz /= CALIBRATION_TICKS;
                    printf("--- CALIBRATION COMPLETE! TEST MOTION NOW. ---\n");
                }
                continue;
            }

            // 2. Remove Offsets
            float zeroed_ax = raw_ax - bias_ax;
            float zeroed_ay = raw_ay - bias_ay;
            float zeroed_gz = raw_gz - bias_gz;

            // 3. Low-Pass EMA Filters
            prev_ax = apply_ema(zeroed_ax, prev_ax, ALPHA_ACCEL);
            prev_ay = apply_ema(zeroed_ay, prev_ay, ALPHA_ACCEL);
            prev_az = apply_ema(raw_az,    prev_az, ALPHA_ACCEL);
            prev_gz = apply_ema(zeroed_gz, prev_gz, ALPHA_GYRO);

            // 4. Deadzone
            float clean_ax = apply_deadzone(prev_ax, DZ_ACCEL_X);
            float clean_ay = apply_deadzone(prev_ay, DZ_ACCEL_Y);
            float clean_gz = apply_deadzone(prev_gz, DZ_GYRO_Z);

            int dx = 0, dy = 0;

            if (prev_az > 7.5f) {
                
                // Suppress Deceleration Recoil on Y (stopping phase)
                if (clean_ay != 0.0f) {
                    if (last_ay_dir != 0.0f && (clean_ay * last_ay_dir < 0.0f)) {
                        clean_ay = 0.0f; // Block negative rebound spike when stopping
                    } else {
                        last_ay_dir = clean_ay;
                    }
                } else {
                    last_ay_dir = 0.0f;
                }

                // Suppress Deceleration Recoil on X
                if (clean_ax != 0.0f) {
                    if (last_ax_dir != 0.0f && (clean_ax * last_ax_dir < 0.0f)) {
                        clean_ax = 0.0f; // Block rebound spike
                    } else {
                        last_ax_dir = clean_ax;
                    }
                } else {
                    last_ax_dir = 0.0f;
                }

                // 5. Integrate Force into Velocity Buffer
                float force_x = (clean_ax * SENSITIVITY_X) + (-clean_gz * SENSITIVITY_GYRO);
                float force_y = (-clean_ay * SENSITIVITY_Y);

                vel_x = (vel_x + force_x) * FRICTION_DAMPING;
                vel_y = (vel_y + force_y) * FRICTION_DAMPING;

                dx = (int)vel_x;
                dy = (int)vel_y;
            } else {
                vel_x = 0.0f;
                vel_y = 0.0f;
                last_ax_dir = 0.0f;
                last_ay_dir = 0.0f;
            }

            move_mouse(dx, dy);
        }
    }

    closesocket(new_socket);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}