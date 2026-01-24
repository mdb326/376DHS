// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    int port = 8080;
    std::string SERVER_IP = "127.0.0.1";
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    // serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid IP address" << std::endl;
        return 1;
    }

    // sending connection request
    if (connect(clientSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return 1;
    }


    // sending data
    const char* message = "Hello, server!";
    send(clientSocket, message, strlen(message), 0);

    // closing socket
    close(clientSocket);

    return 0;
}