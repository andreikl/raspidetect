V4L = 1
# TODO: to check
H264_ENCODER_RASPBERRY = 0
H264_ENCODER_JETSON = 0
# It allows to run test on platform where h264 Jetson encoder isn't available
# the test code generates static h264 buffers which were recorder on Jetson platform
H264_ENCODER_JETSON_WRAP = 1
CONTROL = 0
RFB = 1
SDL = 1
CMOCKA = 1

OPENCV = 0
OPENVG = 0
TENSORFLOW = 0
DARKNET = 0

EXEC = raspidetect
export SRC_DIR ?= ./src
export BUILD_DIR ?= ./build

#                                                          execution time|code size|memory usage|compile time
#-O0 		optimization for compilation time (default) 		+ 	+ 	- 	-
#-O1 or -O 	optimization for code size and execution time 		- 	- 	+ 	+
#-O2 		optimization more for code size and execution time 	-- 	  	+ 	++
#-O3 		optimization more for code size and execution time 	--- 	  	+ 	+++
#-Os 		optimization for code size 	  				-- 	  	++
#-Ofast 	O3 with fast none accurate math calculations 		--- 	  	+ 	+++

#CC = arm-linux-gnueabihf-gcc
#-mcpu=arm6 -mfpu=vfp
CC = cc
LDFLAGS = -lm
COMMON = -Iexternal/klib/ -Isrc/
#-D_POSIX_C_SOURCE=199309L fixes CLOCK_REALTIME error on pi zero 
#CFLAGS = -pthread -O3 -fPIC -Wall -Wno-implicit-function-declaration -Wno-unused-function -DNDEBUG -std=c11 -D_POSIX_C_SOURCE=199309L
CFLAGS = -pthread -O3 -fPIC -Wall -Wno-implicit-function-declaration -Wno-unused-function -DNDEBUG -std=c11 -D_POSIX_C_SOURCE=199309L

ifeq ($(V4L), 1) 
	COMMON += -DV4L
	COMMON += `pkg-config --cflags libv4l2`
	LDFLAGS += `pkg-config --libs libv4l2`
	OBJ += v4l.o
endif

ifeq ($(H264_ENCODER_RASPBERRY), 1) 
	COMMON += -DMMAL_ENCODER
	COMMON += `pkg-config --cflags mmal`
	LDFLAGS += `pkg-config --libs mmal`
	OBJ += mmal_encoder.o
endif

ifeq ($(H264_ENCODER_JETSON), 1)
	COMMON += -DV4L_ENCODER
	COMMON += `pkg-config --cflags libv4l2`
	LDFLAGS += `pkg-config --libs libv4l2`
	LDFLAGS += -L/usr/lib/aarch64-linux-gnu/tegra -L/usr/lib/aarch64-linux-gnu
	LDFLAGS += -lnvbuf_utils
	OBJ += v4l_encoder.o
endif

ifeq ($(H264_ENCODER_JETSON_WRAP), 1)
	COMMON += -DV4L_ENCODER -DV4L_ENCODER_WRAP
	OBJ += v4l_encoder.o
	ifeq ($(CMOCKA), 1)
		TEST_LDFLAGS += -Wl,--wrap=v4l2_open
		TEST_LDFLAGS += -Wl,--wrap=v4l2_ioctl
		TEST_LDFLAGS += -Wl,--wrap=mmap
		TEST_LDFLAGS += -Wl,--wrap=munmap
	endif
endif

ifeq ($(CONTROL), 1) 
	COMMON += -DCONTROL
	OBJ += control.o
endif

ifeq ($(RFB), 1) 
	COMMON += -DRFB
	OBJ += rfb.o
endif

ifeq ($(SDL), 1) 
	COMMON += -DSDL
	COMMON += `pkg-config --cflags sdl2`
	LDFLAGS += `pkg-config --libs sdl2`
	OBJ += sdl.o
endif

ifeq ($(OPENVG), 1) 
	COMMON += -DOPENVG `pkg-config --cflags freetype2`
	LDFLAGS +=  `pkg-config --libs freetype2` -lbrcmEGL -lbrcmGLESv2
	OBJ += openvg.o 
endif

ifeq ($(OPENCV), 1) 
	COMMON += -DOPENCV
	COMMON += `pkg-config --cflags opencv4` 
	LDFLAGS += -lopencv_imgproc -lopencv_highgui -lopencv_core -latomic -lstdc++
endif

ifeq ($(TENSORFLOW), 1) 
	COMMON += -DTENSORFLOW
	COMMON += -I../tf/tensorflow 
	LDFLAGS += -L/home/pi/tf/tensorflow/tensorflow/lite/tools/make/gen/rpi_armv7l/lib -ltensorflow-lite -lstdc++
	OBJ += tensorflow.o 
else ifeq ($(DARKNET), 1)
	COMMON += -DDARKNET
	COMMON += -I../darknet
	LDFLAGS += -L/home/pi/darknet -ldarknet
	OBJ += darknet.o 
endif

ifeq ($(CMOCKA), 1) 
	TEST_COMMON = $(COMMON) -DCMOCKA `pkg-config --cflags cmocka`
	TEST_LDFLAGS += $(LDFLAGS) `pkg-config --libs cmocka`
#	stat
	TEST_LDFLAGS += -Wl,--wrap=__xstat
	TEST_LDFLAGS += -Wl,--wrap=open
	TEST_LDFLAGS += -Wl,--wrap=close
	TEST_LDFLAGS += -Wl,--wrap=select
	TEST_LDFLAGS += -Wl,--wrap=ioctl
# SDL	
	TEST_LDFLAGS += -Wl,--wrap=SDL_Init
	TEST_LDFLAGS += -Wl,--wrap=SDL_CreateWindow
	TEST_LDFLAGS += -Wl,--wrap=SDL_DestroyWindow
	TEST_LDFLAGS += -Wl,--wrap=SDL_GetWindowSurface
	TEST_LDFLAGS += -Wl,--wrap=SDL_FreeSurface
	TEST_LDFLAGS += -Wl,--wrap=SDL_CreateRGBSurfaceFrom
#	SDL_BlitSurface
	TEST_LDFLAGS += -Wl,--wrap=SDL_UpperBlit
	TEST_LDFLAGS += -Wl,--wrap=SDL_UpdateWindowSurface
	TEST_OBJ = test.o
	TESTS = $(addprefix ${BUILD_DIR}/obj/, $(TEST_OBJ))
endif


OBJ += app.o utils.o file.o yuv_converter.o
OBJS = $(addprefix ${BUILD_DIR}/obj/, $(OBJ))


all: clean setup $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(EXEC)_test

$(BUILD_DIR)/$(EXEC): ${BUILD_DIR}/obj/main.o $(OBJS)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(EXEC)_test: $(OBJS) $(TESTS)
	$(CC) $(TEST_COMMON) $(CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

${BUILD_DIR}/obj/%.o: $(SRC_DIR)/%.c
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

.PHONY: setup
setup:
	mkdir -p ${BUILD_DIR}
	mkdir -p ${BUILD_DIR}/obj

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

