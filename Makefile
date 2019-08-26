MMAL = 1
CAIRO = 1
OPENCV = 1
TENSORFLOW = 1
DARKNET = 0

EXEC = OV5647
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
CFLAGS = -pthread -O3 -Wall -Wno-implicit-function-declaration -Wno-unused-function -DNDEBUG -D_POSIX_C_SOURCE=199309L
OBJ = utils.o main.o

ifeq ($(CAIRO), 1) 
    COMMON += -DCAIRO
    COMMON += `pkg-config --cflags cairo` 
    LDFLAGS += `pkg-config --libs cairo`
    OBJ += overlay.o 
endif

ifeq ($(MMAL), 1) 
    COMMON += -DMMAL
    COMMON += `pkg-config --cflags mmal` 
    LDFLAGS += `pkg-config --libs mmal`
    OBJ += ov5647_helpers.o ov5647.o
endif

ifeq ($(OPENCV), 1) 
    COMMON += -DOPENCV
    COMMON += `pkg-config --cflags opencv4` 
    LDFLAGS += -lopencv_imgproc -lopencv_core -latomic -lstdc++
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
