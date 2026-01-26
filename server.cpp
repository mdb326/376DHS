#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "DHS.hpp"
#include <fstream>
#include <vector>

// std::vector<uint8_t> serializeString()

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

int main() {
    DHS<int> map = DHS<int>();
    // std::vector<std::string> processIPS = getProcesses("config.txt");
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

    // listening to the assigned socket
    listen(server_fd, 5);

    int cnt = 0;
    // accepting connection request
    while (true){
        int clientSocket
            = accept(server_fd, nullptr, nullptr);

        
        cnt++;
        // recieving data
        char buffer[1024] = { 0 };
        recv(clientSocket, buffer, sizeof(buffer), 0);
        std::cout << "Message from client: " << buffer
                << std::endl;

        if (buffer[0] == 'G'){
            //get
            //serialization: 0 for null, 1 for not null
            //4 bytes for length of value?
            //value
            int res = map.get(atoi(&buffer[3]));
            if(res == NULL){
                std::string socketVal = "0";
                send(clientSocket, socketVal.c_str(), socketVal.length(), 0);
            }
            else{
                send(clientSocket, ("1"+std::to_string(res)).c_str(), std::to_string(res).length()+1, 0);
            }
            
        }
        else if (buffer[0] == 'P'){    
            //put
            std::string s = std::string(buffer);
            std::string keyString = s.substr(3, s.find('|'));
            std::string valString = s.substr(s.find('|')+1);

            std::cout << keyString << std::endl;

            bool res = map.put(stoi(keyString), stoi(valString));
            send(clientSocket, std::to_string(res).c_str(), std::to_string(res).length(), 0);
        }

        // closing the socket.
        close(clientSocket);
        if (cnt == 1000){
            break;
        }
    }
    
    close(server_fd);

    return 0;

}