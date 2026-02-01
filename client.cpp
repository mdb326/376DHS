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
#include <chrono>
#include <signal.h>
#include <algorithm>


template <typename T>
std::vector<uint8_t> serialize_raw(const T& val) {
    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &val, sizeof(T));
    return bytes;
}
bool recv_all(int sock, void* buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t r = recv(sock, (char*)buf + received, len - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}
std::vector<std::string> getProcesses(std::string filename, int* operations, int* keys){
    std::ifstream config("config.txt");
    std::string line;
    std::vector<std::string> result;
    std::getline(config, line);
    *operations = std::stoi(line);
    std::getline(config, line);
    *keys = std::stoi(line);

    while (std::getline(config, line)) {
        if (line == "Servers:"){
            continue;
        }
        result.push_back(line);
    }

    config.close(); 
    return result;
}
int get_val(int key, std::string serverIP, int serverPort, int clientSocket){
    std::vector<uint8_t> message;
    message.reserve(5);
    message.push_back('G');
    uint8_t buf[4];
    int net_key = htonl(key);
    std::memcpy(buf, &net_key, 4);
    message.insert(message.end(), buf, buf + 4);
    send(clientSocket, message.data(), message.size(), 0);

    char status;
    recv(clientSocket, &status, 1, 0);
    if (status == '0'){
        return NULL;
    }
    int netLen;
    recv_all(clientSocket, &netLen, sizeof(netLen));
    int len = ntohl(netLen);
    
    std::vector<uint8_t> value(len);
    recv_all(clientSocket, value.data(), len);
    
    int result;
    std::memcpy(&result, value.data(), sizeof(result));


    return ntohl(result);
}
int put_val(int key, std::string val, std::string serverIP, int serverPort, int clientSocket){
    std::vector<uint8_t> message;
    message.reserve(13);
    //P
    message.push_back('P');
    //key
    uint8_t buf[4];
    int net_key = htonl(key);
    std::memcpy(buf, &net_key, 4);
    message.insert(message.end(), buf, buf + 4);
    //length(4)
    int len = htonl(val.length());
    std::memcpy(buf, &len, 4);
    message.insert(message.end(), buf, buf + 4);
    //val
    message.insert(message.end(), val.begin(), val.end());

    send(clientSocket, message.data(), message.size(), 0);

    char buffer[1] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);

    // closing socket
    return buffer[0];
}
int generateRandomInteger(int min, int max) {
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::uniform_int_distribution<> distrib(min, max); // Create uniform int dist between min and max (inclusive)

    return distrib(gen); // Generate random number from the uniform int dist (inclusive)
}
int generateRandomNormalInteger(int min, int max) {
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::normal_distribution<> distrib(max/2, max/4); // Create uniform int dist between min and max (inclusive)

    return std::clamp(std::lround(distrib(gen)), static_cast<long int>(min), static_cast<long int>(max)); // Generate random number from the normal int dist (inclusive)
}
float generateRandomFloat(int min, int max){
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::uniform_real_distribution<float> distrib(min, max); // Create uniform int dist between min and max (inclusive)

    return distrib(gen);
}
std::string generateRandomString(int length){
    std::string res = "";
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()";
    for(int i =0; i < length; i++){
        res += characters[generateRandomInteger(0, 58)];
    }
    return res;
}



int main(){
    signal(SIGPIPE, SIG_IGN); //stop killing server
    int operations = 1000;
    int keys = 10;
    std::vector<std::string> processes = getProcesses("config.txt", &operations, &keys);
    int port = 1895;
    
    std::string SERVER_IP = processes[0];
    int successful_puts = 0;
    int failed_puts = 0;
    int successful_gets = 0;
    int failed_gets = 0;

    //start server here
    // pid_t pid = fork();
    // if (pid == 0){
    //     //child
    //     char* const args[] = {
    //         (char*)"./server",
    //         nullptr
    //     };
    //     execv("./server", args);
    //     _exit(0);
    // }
    
    // barrier
    std::vector<int> sockets;
    sockets.reserve(processes.size());
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
                sockets.push_back(clientSocket);
                break;
            }
            close(clientSocket);
        }
    }

    std::cout << "Starting now" << std::endl;
    
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < operations; i++){
        // std::cout << i << std::endl;
        int key = generateRandomInteger(1, keys);
        // std::cout << key << std::endl;
        int index = key % processes.size();
        SERVER_IP = processes[index];
        int socket = sockets[index];
        if (generateRandomInteger(1,5) == 1){
            std::string val = generateRandomString(200);
            int res = put_val(key, val, SERVER_IP, port, socket);
            if(res){
                successful_puts++;
            }
            else{
                failed_puts++;
            }
        }
        else{
            if (get_val(key, SERVER_IP, port, socket) == NULL){
                failed_gets++;
            }
            else{
                successful_gets++;
            }
        }
    }
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> message = {'C'};
    for (auto process : sockets){
        send(process, message.data(), message.size(), 0);
    }
    
    std::chrono::duration<double> exec_time_i = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "Finished in " << exec_time_i.count() << " Seconds" << std::endl;
    

    std::cout << "Successful Gets: " << successful_gets << std::endl;
    std::cout << "Failed Gets: " << failed_gets << std::endl;
    std::cout << "Successful Puts: " << successful_puts << std::endl;
    std::cout << "Failed Puts: " << failed_puts << std::endl;

    return 0;
}