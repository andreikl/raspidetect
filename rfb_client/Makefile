#window rendering
ENABLE_D3D = 1
ENABLE_DXVA = 0
ENABLE_RFB = 1
ENABLE_H264 = 0
ENABLE_H264_SLICE = 0
ENABLE_CUDA = 0
ENABLE_FFMPEG = 0
ENABLE_FFMPEG_DXVA2 = 1
CMOCKA = 1

EXEC = rfb_client
OBJDIR = ./obj/
SRCDIR = ./src/
#                                                          execution time|code size|memory usage|compile time
#-O0 		optimization for compilation time (default) 		+ 	+ 	- 	-
#-O1 or -O 	optimization for code size and execution time 		- 	- 	+ 	+
#-O2 		optimization more for code size and execution time 	-- 	  	+ 	++
#-O3 		optimization more for code size and execution time 	--- 	  	+ 	+++
#-Os 		optimization for code size 	  				-- 	  	++
#-Ofast 	O3 with fast none accurate math calculations 		--- 	  	+ 	+++

#CC = arm-linux-gnueabihf-gcc
#-mcpu=arm6 -mfpu=vfp
CC = clang

LDFLAGS = -Llib/x64 -Llib
#COBJMACROS - allow c calls
COMMON = -I../external/klib -DCOBJMACROS
#-D_POSIX_C_SOURCE=199309L fixes some errors
CFLAGS = -pthread -O3 -fPIC -Wall -Wno-unused-function -DNDEBUG -std=c17 -D_POSIX_C_SOURCE=199309L
OBJ = utils.o file.o app.o
OBJ_EXE = $(OBJDIR)main.o 

ifeq ($(ENABLE_RFB), 1) 
	COMMON += -DENABLE_RFB
	LDFLAGS += -lws2_32 -lmswsock
	OBJ += rfb.o
endif

ifeq ($(ENABLE_D3D), 1) 
	COMMON += -DENABLE_D3D
	LDFLAGS += -luuid -lole32 -ld3d9
	OBJ += d3d.o
endif

ifeq ($(ENABLE_DXVA), 1) 
	COMMON += -DENABLE_DXVA
	#uuid - useful uuids, ole32 - useful CoTaskMemFree, d3d9 - d3d9, dxva2 - dxva2
	LDFLAGS += -luuid -lole32 -ldxva2
	OBJ += dxva.o
endif

ifeq ($(ENABLE_H264), 1) 
	COMMON += -DENABLE_H264 -I./src/h264
	OBJ += h264.o
endif

ifeq ($(ENABLE_H264_SLICE), 1)
	COMMON += -DENABLE_H264_SLICE
endif 

ifeq ($(ENABLE_CUDA), 1)
	COMMON += -Ilib/include -DENABLE_CUDA
	LDFLAGS += -lcuda -lnvcuvid
	OBJ += cuda.o
endif

ifeq ($(ENABLE_FFMPEG), 1)
	COMMON += -DENABLE_FFMPEG
	COMMON += `pkg-config --cflags libavcodec libavutil` -IC:\cygwin\usr\local\include 
	LDFLAGS += `pkg-config --libs libavcodec libavutil`
	OBJ += ffmpeg.o
endif

ifeq ($(ENABLE_FFMPEG_DXVA2), 1)
	COMMON += -DENABLE_FFMPEG_DXVA2
	COMMON += `pkg-config --cflags libavcodec libavutil` -IC:\cygwin\usr\local\include 
	LDFLAGS += `pkg-config --libs libavcodec libavutil`
	OBJ += ffmpeg_dxva2.o
endif

ifeq ($(CMOCKA), 1) 
	TEST_COMMON = $(COMMON) -DCMOCKA `pkg-config --cflags cmocka`
	TEST_LDFLAGS += $(LDFLAGS) `pkg-config --libs cmocka`
	OBJ_TEST = $(OBJDIR)test.o
endif

OBJS = $(addprefix $(OBJDIR), $(OBJ))

all: clean obj $(EXEC) $(EXEC)_test

$(EXEC): $(OBJS) $(OBJ_EXE)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(EXEC)_test: $(OBJS) $(OBJ_TEST)
	$(CC) $(COMMON) $(TEST_COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(TEST_LDFLAGS)

$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

obj:
	mkdir obj

.PHONY: clean

#clean:
#	powershell if (Test-Path -Path ./obj) { rm -Recurse ./obj }

clean:
	rm -rf $(OBJDIR)
