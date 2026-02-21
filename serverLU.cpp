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
#include <map>
#include <random>

int generateRandomInteger(int min, int max) {
    thread_local static std::random_device rd; // creates random device (unique to each thread to prevent race cons) (static to avoid reinitialization)
    thread_local static std::mt19937 gen(rd());  // Seeding the RNG (unique to each thread to prevent race cons) (static to avoid reinitialization)
    std::uniform_int_distribution<> distrib(min, max); // Create uniform int dist between min and max (inclusive)

    return distrib(gen); // Generate random number from the uniform int dist (inclusive)
}
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
bool send_all(int sock, const void* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t s = send(sock, (const char*)buf + sent, len - sent, 0);
        if (s <= 0) return false;
        sent += s;
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

    // std::cout << "Connected to server: " << ip << std::endl;
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
    uint32_t net_key = htonl(key);
    uint32_t net_operation = htonl(operation);
    std::vector<uint8_t> buffer;
    buffer.reserve(1 + sizeof(net_key) + sizeof(net_operation));

    char op_code = 'L';
    buffer.push_back(op_code);

    uint8_t* pKey = reinterpret_cast<uint8_t*>(&net_key);
    buffer.insert(buffer.end(), pKey, pKey + sizeof(net_key));

    uint8_t* pOp = reinterpret_cast<uint8_t*>(&net_operation);
    buffer.insert(buffer.end(), pOp, pOp + sizeof(net_operation));

    if (!send_all(sock, buffer.data(), buffer.size())) {
        std::cerr << "Failed to send lock message\n";
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
    uint32_t net_key = htonl(key);
    uint32_t net_operation = htonl(operation);
    std::vector<uint8_t> buffer;
    buffer.reserve(1 + sizeof(net_key) + sizeof(net_operation));

    char op_code = 'U';
    buffer.push_back(op_code);

    uint8_t* pKey = reinterpret_cast<uint8_t*>(&net_key);
    buffer.insert(buffer.end(), pKey, pKey + sizeof(net_key));

    uint8_t* pOp = reinterpret_cast<uint8_t*>(&net_operation);
    buffer.insert(buffer.end(), pOp, pOp + sizeof(net_operation));

    if (!send_all(sock, buffer.data(), buffer.size())) {
        std::cerr << "Failed to send lock message\n";
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

    if (!send_all(sock, message.data(), message.size())) {
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
void dealWithSocket(int clientSocket, DHSList& map, std::atomic<int>& operationCounter, int myIndex, int operations, int replication, 
                    const std::vector<std::string>& processIPS, int port) {
    //keep open until we get a c
    while(true){
        char op;
        if (!recv_all(clientSocket, &op, 1)) {
            std::cout << "recv_all failed for socket " << clientSocket << std::endl;
            close(clientSocket);
            return;
        }
        // std::cout << "Received op='" << op << "' from socket=" << clientSocket << std::endl;
        // recieving data
        // std::cout << op << std::endl;
        if (op == 'G') {
            //get
            
            //figure out how to allow for simultaneous reads tho
            int net_key;
            if (!recv_all(clientSocket, &net_key, 4)) {
                close(clientSocket);
                return;
            }
            int key = ntohl(net_key);
            
            int currentOperation = ((myIndex + 1) << 28) | (operationCounter.fetch_add(1) & 0x0FFFFFFF);
            map.getLock(key, currentOperation); 
            auto res = map.get(key);
            map.unLock(key, currentOperation); 

            if (res.size() == 0) {
                char zero = '0';
                if (!send_all(clientSocket, &zero, 1)) {
                    std::cerr << "Failed to send '0' acknowledgment\n";
                    return;
                }
            } else {
                std::vector<uint8_t> msg;
                int len = htonl(res.size());
                msg.reserve(1 + sizeof(len) + res.size());
                msg.push_back('1');
                uint8_t* p = reinterpret_cast<uint8_t*>(&len);
                msg.insert(msg.end(), p, p + sizeof(len));
                msg.insert(msg.end(), res.begin(), res.end());
                if (!send_all(clientSocket, msg.data(), msg.size())) {
                    std::cerr << "Failed to send get message\n";
                    return;
                }
            }
        }
        else if (op == 'P') {
            //put
            int net_key;
            if (!recv_all(clientSocket, &net_key, 4)) { close(clientSocket); return; }
            int key = ntohl(net_key);
            int net_len;
            if (!recv_all(clientSocket, &net_len, 4)) { close(clientSocket); return; }
            int len = ntohl(net_len);
            std::vector<uint8_t> value(len);
            if (!recv_all(clientSocket, value.data(), len)) { close(clientSocket); return; }

            std::vector<int> replications = getReplicationMapping(key, myIndex, replication, processIPS.size());
            int currentOperation = operationCounter.fetch_add(1) + ((myIndex + 1) << 28);

            std::map<int, int> repl_socks;
            for (auto nodeID : replications) {
                if (nodeID != myIndex)
                    repl_socks[nodeID] = connect_to_server(processIPS[nodeID], port);
            }

            for (auto nodeID : replications) {
                if (nodeID == myIndex) {
                    while (!map.getLock(key, currentOperation)) {}
                } else {
                    sendLock(repl_socks[nodeID], key, currentOperation);
                }
            }

            bool ok = false;
            for (auto nodeID : replications) {
                if (nodeID == myIndex) {
                    ok = map.put(key, value);
                    map.unLock(key, currentOperation);
                } else {
                    sendAdjust(repl_socks[nodeID], key, value, currentOperation);
                }
            }


            for (auto& [id, sock] : repl_socks) {
                uint8_t close_msg = 'C';
                send_all(sock, &close_msg, 1);
                close(sock);
            }

            uint8_t ack = ok ? 1 : 0;
            if (!send_all(clientSocket, &ack, 1)) {
                std::cerr << "Failed to send acknowledgment\n";
                return;
            }
        }
        else if (op == 'A'){
            int net_operation, net_key, net_len;
            if (!recv_all(clientSocket, &net_operation, 4)) {
                close(clientSocket);
                return;
            }
            int operation = ntohl(net_operation);
            
            if (!recv_all(clientSocket, &net_key, 4)) {
                close(clientSocket);
                return;
            }
            int key = ntohl(net_key);
 
            if (!recv_all(clientSocket, &net_len, 4)) {
                close(clientSocket);
                return;
            }
            int len = ntohl(net_len);

            std::vector<uint8_t> value(len);
            if (!recv_all(clientSocket, value.data(), len)) {
                close(clientSocket);
                return;
            }

            bool ok = map.put(key, value);
            if(ok){
                ok = map.unLock(key, operation);
            }
            uint8_t ack = ok ? 1 : 0;
            if (!send_all(clientSocket, &ack, 1)) {
                std::cerr << "Failed to send acknowledgment\n";
                return;
            }
            // close(clientSocket);
            // return;
        }
        else if (op == '3') {
            std::vector<int> keys(3);
            std::vector<std::vector<uint8_t>> vals(3);

            for(int j = 0; j < 3; j++){
                int net_key, net_len;
                if (!recv_all(clientSocket, &net_key, 4) ||
                    !recv_all(clientSocket, &net_len, 4)) {
                    close(clientSocket);
                    return;
                }
                keys[j] = ntohl(net_key);
                int len = ntohl(net_len);
                vals[j].resize(len);
                if (!recv_all(clientSocket, vals[j].data(), len)) {
                    close(clientSocket);
                    return;
                }
            }

            std::vector<int> lockOrder = {0, 1, 2};
            std::sort(lockOrder.begin(), lockOrder.end(), [&keys](int a, int b){
                return keys[a] < keys[b];
            });

            std::vector<std::vector<int>> allReplications(3);
            for (int j = 0; j < 3; j++){
                allReplications[j] = getReplicationMapping(keys[j], myIndex, replication, processIPS.size());
            }

            int currentOperation = ((myIndex + 1) << 28) | (operationCounter.fetch_add(1) & 0x0FFFFFFF);

            std::map<int, int> repl_socks;
            for (int j = 0; j < 3; j++) {
                for (auto nodeID : allReplications[j]) {
                    if (nodeID != myIndex && repl_socks.find(nodeID) == repl_socks.end()) {
                        repl_socks[nodeID] = connect_to_server(processIPS[nodeID], port);
                    }
                }
            }

            bool allAcquired = false;
            const int maxRetries = 10;

            for(int attempt = 0; attempt < maxRetries && !allAcquired; attempt++){
                allAcquired = true;
                std::vector<std::pair<int,int>> acquiredLocks;

                for(int j : lockOrder){
                    for(auto nodeID : allReplications[j]){
                        bool gotLock = false;

                        if(nodeID == myIndex){
                            gotLock = map.getLock(keys[j], currentOperation);
                        } else {
                            gotLock = sendLock(repl_socks[nodeID], keys[j], currentOperation);
                        }

                        if(gotLock){
                            acquiredLocks.emplace_back(j, nodeID);
                        } else {
                            for(auto it = acquiredLocks.rbegin(); it != acquiredLocks.rend(); ++it){
                                int keyIdx = it->first;
                                int n = it->second;
                                if(n == myIndex){
                                    map.unLock(keys[keyIdx], currentOperation);
                                } else {
                                    sendUnlock(repl_socks[n], keys[keyIdx], currentOperation);
                                }
                            }
                            allAcquired = false;
                            std::this_thread::sleep_for(std::chrono::milliseconds(2 + generateRandomInteger(1,10)));
                            break;
                        }
                    }
                    if(!allAcquired) break;
                }
            }

            if(!allAcquired){
                for(auto& [id, sock] : repl_socks){
                    uint8_t close_msg = 'C';
                    send_all(sock, &close_msg, 1);
                    close(sock);
                }
                char zero = '0';
                send_all(clientSocket, &zero, 1);
                continue;
            }

            bool ok = false;
            for(int j : lockOrder){
                for(auto nodeID : allReplications[j]){
                    if(nodeID == myIndex){
                        ok = map.put(keys[j], vals[j]);
                        map.unLock(keys[j], currentOperation);
                    } else {
                        sendAdjust(repl_socks[nodeID], keys[j], vals[j], currentOperation);
                        sendUnlock(repl_socks[nodeID], keys[j], currentOperation);
                    }
                }
            }
            // for(int j : lockOrder){
            //     for(auto nodeID : allReplications[j]){
            //         if(nodeID == myIndex){
            //             map.unLock(keys[j], currentOperation);
            //         } else {
            //             sendUnlock(repl_socks[nodeID], keys[j], currentOperation);
            //         }
            //     }
            // }

            for(auto& [id, sock] : repl_socks){
                uint8_t close_msg = 'C';
                send_all(sock, &close_msg, 1);
                close(sock);
            }

            uint8_t ack = ok ? 1 : 0;
            if(!send_all(clientSocket, &ack, 1)){
                std::cerr << "Failed to send acknowledgment\n";
                return;
            }
        }

        else if (op == 'C'){
            close(clientSocket);
            return;
        }
        else if(op == 'L'){
            //lock this key place
            int net_key;
            if (!recv_all(clientSocket, &net_key, 4)) {
                close(clientSocket);
                return;
            }
            int key = ntohl(net_key);
            int netoperation;
            if (!recv_all(clientSocket, &netoperation, 4)) {
                close(clientSocket);
                return;
            }
            int operation = ntohl(netoperation);

            // FD_CLR(clientSocket, &master);
            // std::thread([clientSocket, key, operation, &map]() {
            bool ok = map.getLock(key, operation);
            uint8_t ack = ok ? 1 : 0;
            if (!send_all(clientSocket, &ack, 1)) {
                std::cerr << "Failed to send acknowledgment\n";
                return;
            }

            // close(clientSocket);
            // return;
            // }).detach(); 
        }
        else if (op == 'U'){
            int net_key;
            if (!recv_all(clientSocket, &net_key, 4)) {
                close(clientSocket);
                return;
            }
            int key = ntohl(net_key);
            int netoperation;
            if (!recv_all(clientSocket, &netoperation, 4)) {
                close(clientSocket);
                return;
            }
            int operation = ntohl(netoperation);
            // FD_CLR(clientSocket, &master);
            // std::thread([clientSocket, key, operation, &map]() {
            bool ok = map.unLock(key, operation);
            uint8_t ack = ok ? 1 : 0;
            if (!send_all(clientSocket, &ack, 1)) {
                std::cerr << "Failed to send acknowledgment\n";
                return;
            }

                // close(clientSocket); 
                // return;
            // }).detach();

        }
    }
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
    DHSList map(keys);
    // DHS map;
    // map.put(502, 15);
    //Cupid: 128.180.120.70
    int port = 4040;
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
    listen(server_fd, 100);

    std::string myIP = processIPS[myIndex];


    // accepting connection request
    while (true) {
        int clientSocket = accept(server_fd, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // std::cout << "Accepted connection, socket=" << clientSocket << std::endl;
        
        std::thread(dealWithSocket, clientSocket, std::ref(map), std::ref(operationCounter),
                   myIndex, operations, replication, std::ref(processIPS), port).detach();
    }
    
    close(server_fd);

    return 0;

}