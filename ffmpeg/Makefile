EXEC = ffmpeg
OBJDIR = ./obj/
SRCDIR = ./

CC = gcc

LDFLAGS = -Llib/x64 -Llib
COMMON = 
CFLAGS = -pthread -O3 -fPIC -Wall -Wno-implicit-function-declaration -Wno-unused-function -DNDEBUG -std=c17 -D_POSIX_C_SOURCE=199309L
OBJ = main1.o

COMMON += -IC:\msys64\usr\local\include -DENABLE_FFMPEG
#LDFLAGS += -LC:\msys64\usr\local\lib -lavformat -lavcodec -lswresample -lavutil -lz -lsecur32 -lws2_32 -lm -luser32 -lbcrypt -liconv -lmfplat -lmfuuid -lole32 -lstrmiids
LDFLAGS += -LC:\msys64\usr\local\lib -lavcodec -lavutil -lbcrypt -liconv

OBJS = $(addprefix $(OBJDIR), $(OBJ))

all: clean obj $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

obj:
	mkdir obj

.PHONY: clean

clean:
	powershell if (Test-Path -Path ./obj) { rm -Recurse ./obj }
