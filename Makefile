MMAL = 0
OPENCV = 0
OPENVG = 0
TENSORFLOW = 0
DARKNET = 0
CONTROL = 0
RFB = 0

EXEC = raspidetect
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
CC = cc
LDFLAGS = -lm
COMMON = -Iexternal/klib -Isrc/
#-D_POSIX_C_SOURCE=199309L fixes CLOCK_REALTIME error on pi zero 
CFLAGS = -pthread -O3 -fPIC -Wall -Wno-implicit-function-declaration -Wno-unused-function -DNDEBUG -std=c11 -D_POSIX_C_SOURCE=199309L
OBJ = utils.o main.o

ifeq ($(CONTROL), 1) 
    COMMON += -DCONTROL
    OBJ += control.o
endif

ifeq ($(RFB), 1) 
    COMMON += -DRFB
    OBJ += rfb.o
endif

ifeq ($(MMAL), 1) 
    COMMON += -DMMAL
    COMMON += `pkg-config --cflags mmal`
    LDFLAGS += `pkg-config --libs mmal`
    OBJ += mmal.o
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

OBJS = $(addprefix $(OBJDIR), $(OBJ))

all: clean obj $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

.PHONY: clean

clean:
	rm -rf $(OBJDIR)
