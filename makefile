hw: 
# 	g++ -g -std=c++17 -fgnu-tm -pthread -O3 client.cpp -o client
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 clientParallel.cpp -o client
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 server.cpp -o server

clean: 
	rm client
	rm server