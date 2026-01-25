// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int get_val(int key, std::string serverIP, int serverPort){
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid IP address" << std::endl;
        return 1;
    }

    if (connect(clientSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return 1;
    }

    std::string message = "Get" + std::to_string(key);
    send(clientSocket, message.c_str(), message.length(), 0);
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from server: " << buffer
                << std::endl;

    // closing socket
    close(clientSocket);
}
int put_val(int key, int val, std::string serverIP, int serverPort){
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid IP address" << std::endl;
        return 1;
    }

    if (connect(clientSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return 1;
    }

    std::string message = "Put" + std::to_string(key) + '|' + std::to_string(val);
    send(clientSocket, message.c_str(), message.length(), 0);
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from server: " << buffer
                << std::endl;

    // closing socket
    close(clientSocket);
}


int main()
{
    int port = 8080;
    std::string SERVER_IP = "127.0.0.1";
    
    get_val(501, SERVER_IP, port);
    get_val(502, SERVER_IP, port);
    put_val(501, 500000, SERVER_IP, port);
    get_val(501, SERVER_IP, port);

    return 0;
}