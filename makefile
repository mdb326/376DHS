hw: 
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 test.cpp -o test

clean: 
	rm test