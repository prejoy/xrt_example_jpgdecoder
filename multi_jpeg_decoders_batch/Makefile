################## software build for XRT Native API code
CXXFLAGS := -std=c++17 -Wno-deprecated-declarations
CXXFLAGS += -I$(XILINX_XRT)/include -I$(XILINX_HLS)/include
CXXFLAGS += -O0 -g3 
LDFLAGS := -L$(XILINX_XRT)/lib
LDFLAGS += $(LDFLAGS) -lxrt_coreutil -luuid 
EXECUTABLE := multi_jpeg_decoder

HOST_SRCS := ./main.cpp ./dev_jpeg_decoder.cpp

all: $(EXECUTABLE)

$(EXECUTABLE): $(HOST_SRCS)
	$(CXX) -o $(EXECUTABLE) $^ $(CXXFLAGS) $(LDFLAGS)


################## clean up
clean:
	rm -rf $(EXECUTABLE)
	
