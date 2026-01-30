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
int put_val(int key, int val, std::string serverIP, int serverPort, int clientSocket){
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
    int len = htonl(4);
    std::memcpy(buf, &len, 4);
    message.insert(message.end(), buf, buf + 4);
    //val
    int net_val = htonl(val);
    std::memcpy(buf, &net_val, 4);
    message.insert(message.end(), buf, buf + 4);

    send(clientSocket, message.data(), message.size(), 0);

    char buffer[1024] = { 0 };
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
    
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < operations; i++){
        // std::cout << i << std::endl;
        int key = generateRandomInteger(1, 10);
        int index = key % processes.size();
        SERVER_IP = processes[index];
        int socket = sockets[index];
        if (generateRandomInteger(1,5) == 1){
            int val = generateRandomInteger(INT_MIN, INT_MAX);
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
    std::chrono::duration<double> exec_time_i = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "Finished in " << exec_time_i.count() << " Seconds" << std::endl;
    
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