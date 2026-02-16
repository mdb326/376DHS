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
#include <algorithm>
#include <thread>
#include <mutex>


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
std::vector<int> getReplicationMapping(int key, int myIndex, int replicationIndex, int processQuantity){
    //simple atm
    std::vector<int> res;
    res.push_back(myIndex);
    for(int i = 0; i < replicationIndex-1; i++){
        myIndex++;
        if (myIndex >= processQuantity){
            myIndex = 0;
        }
        res.push_back(myIndex);
    }
    //ensure consistent locking order
    std::sort(res.begin(), res.end());
    return res;
}
bool sendLock(int sock, int key, int operation) {
    char op_code = 'L';
    if (send(sock, &op_code, 1, 0) != 1) {
        std::cerr << "Failed to send operation code\n";
        return false;
    }

    uint32_t net_key = htonl(key);
    uint32_t net_operation = htonl(operation);

    if (send(sock, &net_key, sizeof(net_key), 0) != sizeof(net_key)) {
        std::cerr << "Failed to send key\n";
        return false;
    }

    if (send(sock, &net_operation, sizeof(net_operation), 0) != sizeof(net_operation)) {
        std::cerr << "Failed to send operation\n";
        return false;
    }

    uint8_t ack;
    if (!recv_all(sock, &ack, 1)) {
        std::cerr << "Failed to receive lock ack\n";
        return false;
    }

    return ack != 0;
}
bool sendUnlock(int sock, int key, int operation) {
    char op_code = 'U';
    if (send(sock, &op_code, 1, 0) != 1) {
        std::cerr << "Failed to send operation code\n";
        return false;
    }

    uint32_t net_key = htonl(key);
    uint32_t net_operation = htonl(operation);

    if (send(sock, &net_key, sizeof(net_key), 0) != sizeof(net_key)) {
        std::cerr << "Failed to send key\n";
        return false;
    }

    if (send(sock, &net_operation, sizeof(net_operation), 0) != sizeof(net_operation)) {
        std::cerr << "Failed to send operation\n";
        return false;
    }

    uint8_t ack;
    if (!recv_all(sock, &ack, 1)) {
        std::cerr << "Failed to receive lock ack\n";
        return false;
    }

    return ack != 0;
}

bool sendAdjust(int sock, int key, const std::vector<uint8_t>& value, int operation) {
    std::vector<uint8_t> message;
    message.reserve(13 + value.size());

    message.push_back('A');

    uint32_t net_key = htonl(key);
    uint32_t net_operation = htonl(operation);
    uint32_t net_len = htonl(static_cast<uint32_t>(value.size()));
    
    message.insert(message.end(), reinterpret_cast<uint8_t*>(&net_operation), reinterpret_cast<uint8_t*>(&net_operation) + 4);
    message.insert(message.end(), reinterpret_cast<uint8_t*>(&net_key), reinterpret_cast<uint8_t*>(&net_key) + 4);
    message.insert(message.end(), reinterpret_cast<uint8_t*>(&net_len), reinterpret_cast<uint8_t*>(&net_len) + 4);
    message.insert(message.end(), value.begin(), value.end());

    if (send(sock, message.data(), message.size(), 0) != (ssize_t)message.size()) {
        std::cerr << "Failed to send adjust message\n";
        return false;
    }

    uint8_t ack;
    if (!recv_all(sock, &ack, 1)) {
        std::cerr << "Failed to receive adjust ack\n";
        return false;
    }

    return ack != 0;
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

    // std::vector<int> outgoingServerSockets; // index matches processIPS
    std::vector<std::mutex> socketMutexes(processIPS.size());


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
                    std::cout << "recv_all failed for socket " << clientSocket << std::endl;
                    close(clientSocket);
                    FD_CLR(clientSocket, &master);
                    continue;
                }
                cnt++;
                std::cout << "Received op='" << op << "' from socket=" << clientSocket << std::endl;
                // recieving data
                std::cout << op << std::endl;
                if (op == 'G') {
                    //get
                    
                    //figure out how to allow for simultaneous reads tho
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    // std::cout << key << std::endl;
                    std::thread([clientSocket, key, &map, &operationCounter, myIndex, operations]() {
                        // operationCounter++; //this is another operation but doesn't affect sole locking, gonna need to 
                        int currentOperation = operationCounter.fetch_add(1) + (myIndex+1)*operations;
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
                    }).detach();
                }
                else if (op == 'P') {
                    //put
                    
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
                    
                    std::thread([clientSocket, key, value, &map, &operationCounter, myIndex, 
                                operations, replication, &processIPS, &socketMutexes, port]() {
                        // operationCounter++; //gonna have to send this with teh first digit identifying the nodeh
                        std::vector<int> replications = getReplicationMapping(key, myIndex, replication, processIPS.size());
                        
                        int currentOperation = operationCounter.fetch_add(1) + (myIndex+1)*operations;
                        
                        for(auto nodeID : replications){
                            if(nodeID == myIndex){
                                while(!map.getLock(key, currentOperation)){}
                                continue;
                            }
                            std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                            int temp_sock = connect_to_server(processIPS[nodeID], port);
                            while(!sendLock(temp_sock, key, currentOperation)){
                            }
                            close(temp_sock);
                            
                        }
                        bool ok;
                        for (auto nodeID : replications){
                            if(nodeID == myIndex){
                                ok = map.put(key, value);
                                continue;
                            }
                            std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                            int temp_sock = connect_to_server(processIPS[nodeID], port);
                            while(!sendAdjust(temp_sock, key, value, currentOperation)){
                            }
                            close(temp_sock);
                        }
                        
                        
                        
                        for(auto nodeID : replications){
                            if(nodeID == myIndex){
                                while(!map.unLock(key, currentOperation)){}
                                continue;
                            }
                            std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                            int temp_sock = connect_to_server(processIPS[nodeID], port);
                            while(!sendUnlock(temp_sock, key, currentOperation)){
                            }
                            close(temp_sock);
                        }

                        send(clientSocket, &ok, 1, 0);
                    }).detach();
                }
                else if (op == 'A'){
                    int net_operation;
                    int net_key;
                    int net_len;

                    recv_all(clientSocket, &net_operation, 4);
                    int operation = ntohl(net_operation);

                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);

                    recv_all(clientSocket, &net_len, 4);
                    int len = ntohl(net_len);

                    std::vector<uint8_t> value(len);
                    recv_all(clientSocket, value.data(), len);

                    //this is an unsafe put op but we are trusting the server to lock before calling it
                    bool ok = map.put(key, value);
                    send(clientSocket, &ok, 1, 0);
                }
                else if (op == '3') {
                    //3put
                    std::vector<int> keys(3);
                    std::vector<std::vector<uint8_t>> vals(3);
                    for(int j = 0; j < 3; j++){
                        int net_key;
                        recv_all(clientSocket, &net_key, 4);
                        int key = ntohl(net_key);
                        keys[j] = key;
                        // std::cout << "put" << std::endl;
                        // std::cout << key << std::endl;

                        int net_len;
                        recv_all(clientSocket, &net_len, 4);
                        int len = ntohl(net_len);

                        std::vector<uint8_t> value(len);
                        recv_all(clientSocket, value.data(), len);
                        vals[j] = value;
                    }
                    
                    
                    std::thread([clientSocket, keys, vals, &map, &operationCounter, myIndex, 
                                operations, replication, &processIPS, &socketMutexes, port]() {
                        // operationCounter++; //gonna have to send this with teh first digit identifying the nodeh
                        std::vector<std::vector<int>> allReplications(3);
                        for (int j = 0; j < 3; j++){
                            allReplications[j] = getReplicationMapping(keys[j], myIndex, replication, processIPS.size());
                        }
                        
                        
                        int currentOperation = operationCounter.fetch_add(1) + (myIndex+1)*operations;
                        for(int j = 0; j < 3; j++){
                            for(auto nodeID : allReplications[j]){
                                if(nodeID == myIndex){
                                    while(!map.getLock(keys[j], currentOperation)){}
                                    continue;
                                }
                                std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                                int temp_sock = connect_to_server(processIPS[nodeID], port);
                                while(!sendLock(temp_sock, keys[j], currentOperation)){
                                }
                                close(temp_sock);
                                
                            }
                        }
                        
                        bool ok;
                        for(int j = 0; j < 3; j++){
                            for (auto nodeID : allReplications[j]){
                            if(nodeID == myIndex){
                                ok = map.put(keys[j], vals[j]);
                                continue;
                            }
                            std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                            int temp_sock = connect_to_server(processIPS[nodeID], port);
                            while(!sendAdjust(temp_sock, keys[j], vals[j], currentOperation)){
                            }
                            close(temp_sock);
                        }
                        }

                        for(int j = 0; j < 3; j++){
                            for(auto nodeID : allReplications[j]){
                            if(nodeID == myIndex){
                                while(!map.unLock(keys[j], currentOperation)){}
                                continue;
                            }
                            std::lock_guard<std::mutex> lock(socketMutexes[nodeID]);
                            int temp_sock = connect_to_server(processIPS[nodeID], port);
                            while(!sendUnlock(temp_sock, keys[j], currentOperation)){
                            }
                            close(temp_sock);
                        }
                        }
                        

                        send(clientSocket, &ok, 1, 0);
                    }).detach();
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

                    // std::thread([clientSocket, key, operation, &map]() {
                        bool ok = map.getLock(key, operation);
                        send(clientSocket, &ok, 1, 0);
                    // }).detach();
                }
                else if (op == 'U'){
                    int net_key;
                    recv_all(clientSocket, &net_key, 4);
                    int key = ntohl(net_key);
                    int netoperation;
                    recv_all(clientSocket, &netoperation, 4);
                    int operation = ntohl(netoperation);
                    // std::thread([clientSocket, key, operation, &map]() {
                        bool ok = map.unLock(key, operation);
                        send(clientSocket, &ok, 1, 0);
                    // }).detach();
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