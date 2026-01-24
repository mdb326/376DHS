hw: 
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 test.cpp -o test
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 client.cpp -o client
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 server.cpp -o server

clean: 
	rm test
	rm client
	rm server