#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

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

    // Initialize Winsock
    WSAStartup(MAKEWORD(2,2), &wsa);

    // Create Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(12345);

    // Bind and Listen on Port 12345
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
        clearConsoleLine();
        printf("--- USB Sensor Stream Connected! ---\n");
        printf("Data: %-50s\n", buffer);
    }

    closesocket(new_socket);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}