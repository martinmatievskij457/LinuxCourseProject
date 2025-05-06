CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -lncurses
OBJS = main.o process_info.o thread_info.o utils.o ui.o

monitor: $(OBJS)
	$(CXX) $(OBJS) -o monitor $(CXXFLAGS)

%.o: %.cpp
	$(CXX) -c $< -o $@

clean:
	rm -f *.o monitor
