hw: 
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 clientUnknown.cpp -o client
# 	g++ -g -std=c++17 -fgnu-tm -pthread -O3 clientCache.cpp -o clientCache
# 	g++ -g -std=c++17 -fgnu-tm -pthread -O3 clientCacheSmall.cpp -o clientCacheSmall
# 	g++ -g -std=c++17 -fgnu-tm -pthread -O3 clientParallel.cpp -o clientParallel
	g++ -g -std=c++17 -fgnu-tm -pthread -O3 serverUnknown.cpp -o server

clean: 
	rm client
	rm server
	rm clientCache
	rm clientCacheSmall
	rm clientParallel