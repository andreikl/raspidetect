// i2c driver node: /dev/i2c-1
const char *node = "/dev/i2c-1";
// OV7670 I2C Address
#define OV7670_ADDRESS 0x21
// Version registers
#define OV7670_PID 0xA
#define OV7670_VER 0xB

#ifdef _WIN32
#include <fcntl.h>
// Get the adapter functionality mask
#define I2C_FUNCS	        0x0705
// Plain i2c-level commands
#define I2C_FUNC_I2C        0x00000001
// read data, from slave to master
#define I2C_M_RD            0x0001
// Combined R/W transfer (one STOP only)
#define I2C_RDWR	0x0707

struct i2c_msg {
    // slave address
    unsigned short addr;
    unsigned short flags;
    // msg length
    unsigned short len;
    // pointer to msg data
    char *buf;
};

struct i2c_rdwr_ioctl_data {
    // pointers to i2c_msgs
    struct i2c_msg *msgs;
    // number of i2c_msgs
    unsigned long nmsgs;
};

#else
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#endif // WINDOWS

// --------------
// i2c driver instance, node: /dev/i2c-1
static int i2c_fd = -1;
// Support flags
static unsigned long i2c_funcs = 0;

// Exit program if signaled
static int is_signaled = 0;

const char* debug_file;

//i2c_open(node);
//ret = i2c_read16(OV7670_ADDRESS, OV7670_PID);
//printf("Version: %d", ret);
//i2c_close();
//const char* input_file = read_str_value(INPUT, INPUT_DEF);
//const char* output_file = read_str_value(OUTPUT, OUTPUT_DEF);
//debug_file = read_str_value(DEBUG, DEBUG_DEF);
//int width = read_int_value(WIDTH, WIDTH_DEF);
//int height = read_int_value(HEIGHT, HEIGHT_DEF);
// --------------

// Signal handler to quit the program
void sigint_handler(int signo) {
    is_signaled = 1;
}


// Read 16-bits of data from i2c device
int i2c_read16(int addr, int reg) {
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[2];
    unsigned char wbuf[1], rbuf[2];
    int rc;

    wbuf[0] = (char)reg;

    iomsgs[0].addr = iomsgs[1].addr = (unsigned short)addr;
    iomsgs[0].flags = 0;		/* Write */
    iomsgs[0].buf = wbuf;
    iomsgs[0].len = 1;

    iomsgs[1].flags = I2C_M_RD; /* Read */
    iomsgs[1].buf = rbuf;		
    iomsgs[1].len = 2;

    msgset.msgs = iomsgs;
    msgset.nmsgs = 2;

    if ((rc = ioctl(i2c_fd, I2C_RDWR, &msgset)) < 0)
        return -1;

    return (rbuf[0] << 8) | rbuf[1];
}


// Read 8-bit value from peripheral at addr :
int i2c_read8(int addr, int reg) {
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[2];
    unsigned char wbuf[1], rbuf[1];
    int rc;

    wbuf[0] = (char)reg;

    iomsgs[0].addr = iomsgs[1].addr = (unsigned short)addr;
    iomsgs[0].flags = 0;		/* Write */
    iomsgs[0].buf = wbuf;
    iomsgs[0].len = 1;

    iomsgs[1].flags = I2C_M_RD;	/* Read */
    iomsgs[1].buf = rbuf;
    iomsgs[1].len = 1;

    msgset.msgs = iomsgs;
    msgset.nmsgs = 2;

    rc = ioctl(i2c_fd, I2C_RDWR, &msgset);

    return rc < 0 ? -1 : ((int)(rbuf[0]) & 0x0FF);
}


// Open I2C bus and check capabilities
void i2c_open(const char *node) {
    int rc;

    // Open driver /dev/i2s-1
    i2c_fd = open(node, O_RDWR);
    if (i2c_fd < 0) {
        perror("Opening /dev/i2s-1");
        abort();
    }
    
    // Make sure the driver supports plain I2C I/O
    rc = ioctl(i2c_fd, I2C_FUNCS, &i2c_funcs);
    assert(rc >= 0);
    assert(i2c_funcs & I2C_FUNC_I2C);
}

// Close the I2C driver
void i2c_close() {
    close(i2c_fd);
    i2c_fd = -1;
}
