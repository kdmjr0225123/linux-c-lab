CFLAGS=-Wall -Wextra
CXXFLAGS=-Wall -Wextra
PROGS=server client server_cpp client_cpp server_multi client_multi server_counter server_timeout server_errors server_broadcast client_broadcast
all: $(PROGS)
server_cpp: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server_cpp
client_cpp: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client_cpp
clean:
	rm -f $(PROGS) server_errors.log
