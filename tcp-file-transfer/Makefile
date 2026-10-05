CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
COMMON   = common/net.h common/sha256.h

all: version1/server version1/client version2_threadpool/server version2_threadpool/client

version1/server: version1/server.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $< -o $@

version1/client: version1/client.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $< -o $@

version2_threadpool/server: version2_threadpool/server.cpp version2_threadpool/threadpool.cpp version2_threadpool/threadpool.h $(COMMON)
	$(CXX) $(CXXFLAGS) version2_threadpool/server.cpp version2_threadpool/threadpool.cpp -o $@

version2_threadpool/client: version2_threadpool/client.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f version1/server version1/client version2_threadpool/server version2_threadpool/client
	rm -rf bench_run

.PHONY: all clean
