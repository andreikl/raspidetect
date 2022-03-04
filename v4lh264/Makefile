
# All common header files
CPPFLAGS = -std=c++11 `pkg-config --cflags libv4l2`

# All common dependent libraries
LDFLAGS = -lpthread `pkg-config --libs libv4l2` -L/usr/lib/aarch64-linux-gnu/tegra -L/usr/lib/aarch64-linux-gnu -lnvbuf_utils


APP := video_encode
CC := g++

SRCS := encoder_unit_sample.cpp

OBJS := $(SRCS:.cpp=.o)

all: $(APP)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CC) $(CPPFLAGS) -c $<

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(CC) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS)

clean:
	$(AT)rm -rf $(APP) $(OBJS)
