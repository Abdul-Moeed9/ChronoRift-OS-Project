CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread -I.

LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt

TARGETS = arbiter.out hip.out asp.out

all: clean $(TARGETS)
	@echo Build complete.

ARB_DIR = BCS\ A_24i_0720_24i_0845_HamzaSheikh_arbiter
HIP_DIR = BCS\ A_24i_0720_24i_0845_HamzaSheikh_hip
ASP_DIR = BCS\ A_24i_0720_24i_0845_HamzaSheikh_asp

arbiter.out: $(ARB_DIR)/arbiter.cpp
	$(CXX) $(CXXFLAGS) $(ARB_DIR)/*.cpp -o arbiter.out $(LIBS)

hip.out: $(HIP_DIR)/hip.cpp
	$(CXX) $(CXXFLAGS) $(HIP_DIR)/*.cpp -o hip.out $(LIBS)

asp.out: $(ASP_DIR)/asp.cpp
	$(CXX) $(CXXFLAGS) $(ASP_DIR)/*.cpp -o asp.out $(LIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
