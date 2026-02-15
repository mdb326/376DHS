#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
// #include "DHS.hpp"
#include <fstream>
#include <vector>
#include <cstring>
#include "serialization.cpp"
#include "DHSList.hpp"
#include <atomic>


std::vector<std::string> getProcesses(std::string filename, int* operations, int* keys, int* replication){
    std::ifstream config("config.txt");
    std::string line;
    std::vector<std::string> result;
    std::getline(config, line);
    *operations = std::stoi(line);
    std::getline(config, line);
    *keys = std::stoi(line);
    std::getline(config, line);
    *replication = std::stoi(line);

    while (std::getline(config, line)) {
        if (line == "Servers:"){
            continue;
        }
        result.push_back(line);
    }

    config.close(); 
    return result;
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
int connect_to_server(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    std::cout << "Connected to server: " << ip << std::endl;
    return sock;
}


int main(int argc, char* argv[]) {
    int myIndex = std::stoi(argv[1]);
    char type = '1';
    int operations = 1000;
    int keys = 10;
    int replication = 1;
    std::atomic<int> operationCounter(0);
    std::vector<std::string> processIPS = getProcesses("config.txt", &operations, &keys, &replication);
    std::vector<bool> locksHeld; //which locks we currently have
    std::vector<std::unique_ptr<std::shared_mutex>> lockLocks; //lock the thing to say if we have a lock
    for (int i = 0; i < processIPS.size(); i++){
        lockLocks.emplace_back(std::make_unique<std::shared_mutex>());
    }
    locksHeld.resize(processIPS.size(), false);
    DHSList map(keys / processIPS.size() + 1);
    // DHS map;
    // map.put(502, 15);
    //Cupid: 128.180.120.70
    int port = 1895;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0){
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    int cnt = 0;
    fd_set  master, readfds;
    FD_ZERO(&master);
    FD_SET(server_fd, &master);
    int maxfd = server_fd;

    // listening to the assigned socket
    listen(server_fd, 10);
    //need to connect to all other servers too
    std::string myIP = processIPS[myIndex];

    for (const auto& ip : processIPS) {
        while (true) {
            int sock = connect_to_server(ip, port);
            if (sock >= 0) {
                FD_SET(sock, &master);
                maxfd = std::max(maxfd, sock);
                break;
            }
            sleep(1);
        }
    }

    // accepting connection request
    while (true){
        readfds = master;

        if (select(maxfd +1, &readfds, nullptr, nullptr, nullptr) < 0){
            break;
        }
        for(int i = 0; i <= maxfd; i++){
            if(!FD_ISSET(i, &readfds)){
                continue;
            }

            if(i == server_fd){
                int client = accept(server_fd, nullptr, nullptr);
                FD_SET(client, &master);
                maxfd = std::max(maxfd, client);
            }
            else{
                // std::cout << cnt << std::endl;
                char op;
                int clientSocket = i;
                if (!recv_all(clientSocket, &op, 1)) {
                    //check if dead
                    close(clientSocket);
                    FD_CLR(clientSocket, &master);
                    continue;
                }
                // recv_all(clientSocket, &op, 1);
                cnt++;
                // recieving data
                if (op == 'G') {
                    //get
                    operationCounter++; //this is another operation but doesn't affect sole locking, gonna need to 
                    //figure out how to allow for simultaneous reads tho
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    // std::cout << key << std::endl;
                    int currentOperation = operationCounter + (myIndex+1)*operations; //uniqueID
                    map.getLock(key, currentOperation); 
                    auto res = map.get(key);
                    map.unLock(key, currentOperation); 

                    if (res.size() == 0) {
                        char zero = '0';
                        send(clientSocket, &zero, 1, 0);
                    } else {
                        std::vector<uint8_t> msg;
                        int len = htonl(res.size());
                        msg.reserve(1 + sizeof(len) + res.size());
                        msg.push_back('1');
                        uint8_t* p = reinterpret_cast<uint8_t*>(&len);
                        msg.insert(msg.end(), p, p + sizeof(len));
                        msg.insert(msg.end(), res.begin(), res.end());
                        send(clientSocket, msg.data(), msg.size(), 0);
                    }
                }
                else if (op == 'P') {
                    //put
                    operationCounter++; //gonna have to send this with teh first digit identifying the nodeh
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    // std::cout << "put" << std::endl;
                    // std::cout << key << std::endl;

                    int net_len;
                    recv_all(clientSocket, &net_len, 4);
                    int len = ntohl(net_len);

                    std::vector<uint8_t> value(len);
                    recv_all(clientSocket, value.data(), len);

                    bool ok = map.put(key, value);

                    send(clientSocket, &ok, 1, 0);
                }
                else if (op == 'C'){
                    close(clientSocket);
                    FD_CLR(clientSocket, &master);
                }
                else if(op == 'L'){
                    //lock this key place
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    int netoperation;
                    recv_all(clientSocket, &netoperation, 4);
                    int operation = ntohl(netoperation);
                    bool ok = map.getLock(key, operation);
                    //send back ack

                    send(clientSocket, &ok, 1, 0);
                }
                else if (op == 'U'){
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    int netoperation;
                    recv_all(clientSocket, &netoperation, 4);
                    int operation = ntohl(netoperation);
                    bool ok = map.unLock(key, operation);
                    //send back ack
                    send(clientSocket, &ok, 1, 0);
                }
            }
        }


        // closing the socket.
        // close(clientSocket);
        // if (cnt == 1000){
        //     break;
        // }
    }
    
    close(server_fd);

    return 0;

}