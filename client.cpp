// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>

int get_val(int key, std::string serverIP, int serverPort){
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid IP address" << std::endl;
        return NULL;
    }

    if (connect(clientSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return NULL;
    }

    std::string message = "Get" + std::to_string(key);
    send(clientSocket, message.c_str(), message.length(), 0);
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from server: " << buffer
                << std::endl;

    
    // closing socket
    close(clientSocket);

    if (buffer[0] == '0'){
        return NULL;
    }
    return std::stoi(std::string(&buffer[1]));
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
int generateRandomInteger(int min, int max) {
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::uniform_int_distribution<> distrib(min, max); // Create uniform int dist between min and max (inclusive)

    return distrib(gen); // Generate random number from the uniform int dist (inclusive)
}


int main()
{
    int port = 8080;
    int operations = 1000;
    std::string SERVER_IP = "127.0.0.1";
    int successful_puts = 0;
    int failed_puts = 0;
    int successful_gets = 0;
    int failed_gets = 0;
    
    for(int i = 0; i < operations; i++){
        int key = generateRandomInteger(INT8_MIN, INT8_MAX);
        if (generateRandomInteger(1,5) == 1){
            int val = generateRandomInteger(INT8_MIN, INT8_MAX);
            put_val(key, val, SERVER_IP, port);
        }
        else{
            if (get_val(key, SERVER_IP, port) == NULL){
                failed_gets++;
            }
            else{
                successful_gets++;
            }
        }
    }
    
    // get_val(501, SERVER_IP, port);
    // get_val(502, SERVER_IP, port);
    // put_val(501, 500000, SERVER_IP, port);
    // get_val(501, SERVER_IP, port);
    // put_val(501, 500001, SERVER_IP, port);
    // get_val(501, SERVER_IP, port);

    std::cout << "Successful Gets: " << successful_gets << std::endl;
    std::cout << "Failed Gets: " << failed_gets << std::endl;

    return 0;
}