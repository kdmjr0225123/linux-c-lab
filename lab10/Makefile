CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
SRCS = main.cpp Process.cpp CPU.cpp Scheduler.cpp TLB.cpp TwoLevelPageTable.cpp Clock.cpp
OBJS = $(SRCS:.cpp=.o)

paging: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp *.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) paging

.PHONY: clean
