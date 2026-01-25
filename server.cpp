#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "DHS.hpp"

int main() {
    DHS<int> map = DHS<int>();
    map.put(502, 15);
    //Cupid: 128.180.120.70
    int port = 8080;
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

    // accepting connection request
    while (true){
        int clientSocket
            = accept(server_fd, nullptr, nullptr);

        // recieving data
        char buffer[1024] = { 0 };
        recv(clientSocket, buffer, sizeof(buffer), 0);
        std::cout << "Message from client: " << buffer
                << std::endl;

        if (buffer[0] == 'G'){
            //get
            int res = map.get(atoi(&buffer[3]));
            if(res == NULL){
                std::cout << "null" << std::endl;
            }
            else{
                send(clientSocket, std::to_string(res).c_str(), std::to_string(res).length(), 0);
            }
            
            std::cout << res << std::endl;
        }
        else if (buffer[0] == 'P'){
            //put
            std::string s = std::string(buffer);
            std::string keyString = s.substr(0, s.find('|'));
            std::string valString = s.substr(s.find('|')+1);

            std::cout << keyString << std::endl;
            std::cout << valString << std::endl;

            bool res = map.put(stoi(keyString), stoi(valString));
            send(clientSocket, std::to_string(res).c_str(), std::to_string(res).length(), 0);
        }

        // closing the socket.
        close(clientSocket);
    }
    
    close(server_fd);

    return 0;

}