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
#include <climits>
#include <vector>
#include <fstream>



std::vector<std::string> getProcesses(std::string filename){
    std::ifstream config("config.txt");
    std::string line;
    std::vector<std::string> result;

    while (std::getline(config, line)) {
        if (line == "Servers:"){
            continue;
        }
        result.push_back(line);
    }

    config.close(); 
    return result;
}
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
        return NULL;
    }

    if (connect(clientSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return NULL;
    }

    std::string message = "Put" + std::to_string(key) + '|' + std::to_string(val);
    send(clientSocket, message.c_str(), message.length(), 0);
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);

    // closing socket
    close(clientSocket);
    return buffer[0] == '1';
}
int generateRandomInteger(int min, int max) {
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::uniform_int_distribution<> distrib(min, max); // Create uniform int dist between min and max (inclusive)

    return distrib(gen); // Generate random number from the uniform int dist (inclusive)
}


int main()
{
    std::vector<std::string> processes = getProcesses("config.txt");
    int port = 1895;
    int operations = 1000;
    std::string SERVER_IP = processes[0];
    int successful_puts = 0;
    int failed_puts = 0;
    int successful_gets = 0;
    int failed_gets = 0;

    //start server here
    pid_t pid = fork();
    if (pid == 0){
        //child
        char* const args[] = {
            (char*)"./server",
            nullptr
        };
        execv("./server", args);
        _exit(0);
    }
    
    // barrier
    for (std::string process : processes){
        std::cout << process << std::endl;
        while(true){
            int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddress{};
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_port = htons(port);

            if (inet_pton(AF_INET, process.c_str(), &serverAddress.sin_addr) <= 0) {
                std::cerr << "Invalid IP address" << std::endl;
                return NULL;
            }

            if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == 0) {
                close(clientSocket);
                break;
            }
            close(clientSocket);
            sleep(1);
        }
    }
    
    for(int i = 0; i < operations; i++){
        int key = generateRandomInteger(1, 10);
        if (generateRandomInteger(1,5) == 1){
            int val = generateRandomInteger(INT_MIN, INT_MAX);
            if(put_val(key, val, SERVER_IP, port)){
                successful_puts++;
            }
            else{
                failed_puts++;
            }
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
    std::cout << "Successful Puts: " << successful_puts << std::endl;
    std::cout << "Failed Puts: " << failed_puts << std::endl;

    return 0;
}