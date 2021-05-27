#include <termio.h> //constants: ICANON, TCSANOW, FIONREAD
#include <fcntl.h> // constants: O_RDWR | O_SYNC
#include <sys/mman.h> //memory mapping
#include <sys/time.h> // timeval 

#include "main.h"

//#define BCM2837_PERI_BASE        0x3F000000
#define BCM2835_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2835_PERI_BASE + 0x200000)  // GPIO controller

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

#define GPIO_A1 27
#define GPIO_A2 22
#define GPIO_B1 23
#define GPIO_B2 24

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(gpio, g) *((gpio) + ((g) / 10)) &= ~(7 << (((g) % 10) * 3))
#define OUT_GPIO(gpio, g) *((gpio) + ((g) / 10)) |=  (1 << (((g) % 10) * 3))
#define SET_GPIO_ALT(gpio, g, a) *((gpio) + (((g) / 10))) |= (((a) <= 3? (a) + 4: (a) == 4? 3: 2) << (((g) % 10) * 3))

#define GPIO_SET(gpio) *(gpio + 7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR(gpio) *(gpio + 10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(gpio, g) (*((gpio) + 13) & (1 << g)) // 0 if LOW, (1 << g) if HIGH

#define GPIO_PULL(gpio) *(gpio + 37) // Pull up/pull down
#define GPIO_PULLCLK0(gpio) *(gpio + 38) // Pull up/pull down cloc

static int kb_left = 0, kb_up = 0, kb_right = 0, kb_down = 0;

// static void set_mode(int want_key)
// {
//     fprintf(stderr, "INFO: set mode: %d\n", want_key);
// 	static struct termios old, new;
// 	if (!want_key) {
// 		tcsetattr(STDIN_FILENO, TCSANOW, &old);
// 		return;
// 	}
 
// 	tcgetattr(STDIN_FILENO, &old);
// 	new = old;
// 	new.c_lflag &= ~(ICANON | ECHO);
// 	tcsetattr(STDIN_FILENO, TCSANOW, &new);
// }

// static int get_key()
// {
// 	int c = 0;
// 	struct timeval tv;
// 	fd_set fs;
// 	tv.tv_usec = tv.tv_sec = 0;
 
// 	FD_ZERO(&fs);
// 	FD_SET(STDIN_FILENO, &fs);
// 	select(STDIN_FILENO + 1, &fs, 0, 0, &tv);
 
// 	if (FD_ISSET(STDIN_FILENO, &fs)) {
// 		c = getchar();
// 		set_mode(0);
// 	}
// 	return c;
// }

static int utils_kbhit(int *x, int *y, int *z)
{
    struct termios original;
    tcgetattr(STDIN_FILENO, &original);

    struct termios term;
    memcpy(&term, &original, sizeof(term));

    term.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    int characters_buffered = 0;
    ioctl(STDIN_FILENO, FIONREAD, &characters_buffered);

    *x = (characters_buffered > 0)? getchar(): 0;
    *y = (characters_buffered > 1)? getchar(): 0;
    *z = (characters_buffered > 2)? getchar(): 0;
    int s = characters_buffered - 3;
    while (s > 0) {
        getchar(); s--;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &original);

    return characters_buffered;
}

int control_init(struct app_state_t *app)
{
    //set_mode(1);

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        fprintf(stderr, "ERROR: Can't open /dev/mem\n");
        return -1;
    }

    // mmap GPIO
    void *gpio_map = mmap(
        NULL,             // Any adddress in our space will do
        BLOCK_SIZE,       // Map length
        PROT_READ | PROT_WRITE, // Enable reading & writting to mapped memory
        MAP_SHARED,       // Shared with other processes
        mem_fd,           // File to map
        GPIO_BASE         // Offset to GPIO peripheral
    );

    close(mem_fd); //No need to keep mem_fd open after mmap

    if (gpio_map == MAP_FAILED) {
        fprintf(stderr, "ERROR: control mmap error %d\n", errno);
        return -1;
    }

    // Always use volatile pointer!
    app->control.gpio = (volatile unsigned *)gpio_map;

    INP_GPIO(app->control.gpio, GPIO_A1); // must use INP_GPIO before we can use OUT_GPIO
    OUT_GPIO(app->control.gpio, GPIO_A1);

    INP_GPIO(app->control.gpio, GPIO_A2);
    OUT_GPIO(app->control.gpio, GPIO_A2);

    INP_GPIO(app->control.gpio, GPIO_B1);
    OUT_GPIO(app->control.gpio, GPIO_B1);

    INP_GPIO(app->control.gpio, GPIO_B2);
    OUT_GPIO(app->control.gpio, GPIO_B2);

    return 0;
}

static void move_forward_start(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_forward_start\n");
    GPIO_SET(app->control.gpio) = (1 << GPIO_A1) | (1 << GPIO_B1);
}

static void move_forward_stop(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_forward_stop\n");
    GPIO_CLR(app->control.gpio) = (1 << GPIO_A1) | (1 << GPIO_B1);
}

static void move_backwards_start(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_backwards_start\n");
    GPIO_SET(app->control.gpio) = (1 << GPIO_A2) | (1 << GPIO_B2);
}

static void move_backwards_stop(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_backwards_stop\n");
    GPIO_CLR(app->control.gpio) = (1 << GPIO_A2) | (1 << GPIO_B2);
}

static void move_left_start(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_left_start\n");
    GPIO_SET(app->control.gpio) = (1 << GPIO_A2) | (1 << GPIO_B1);
}

static void move_left_stop(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_left_stop\n");
    GPIO_CLR(app->control.gpio) = (1 << GPIO_A2) | (1 << GPIO_B1);
}

static void move_right_start(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_right_start\n");
    GPIO_SET(app->control.gpio) = (1 << GPIO_A1) | (1 << GPIO_B2);
}

static void move_right_stop(struct app_state_t *app)
{
    fprintf(stderr, "INFO: move_right_stop\n");
    GPIO_CLR(app->control.gpio) = (1 << GPIO_A1) | (1 << GPIO_B2);
}

static int control_stop_all(struct app_state_t *app)
{
    if (kb_left) {
        kb_left = 0;
        move_left_stop(app);
    }
    if (kb_up) {
        kb_up = 0;
        move_forward_stop(app);
    }
    if (kb_right) {
        kb_right = 0;
        move_right_stop(app);
    }
    if (kb_down) {
        kb_down = 0;
        move_backwards_stop(app);
    }
    return 0;
}

int control_ssh_key(struct app_state_t *app)
{
    int x;
    int y;
    int z;
    int size = utils_kbhit(&x, &y, &z);
    int is_left = 0, is_up = 0, is_right = 0, is_down = 0;    
    if (size > 0) {
        fprintf(stderr, "INFO: Key %d, %d, %d, %d\n", x, y, z, size);

        if (x == 97) {
            is_left = 1;
        }
        else if (x == 119) {
            is_up = 1;
        }
        else if (x == 100) {
            is_right = 1;
        }
        else if (x == 115) {
            is_down = 1;
        } else {
        }

        if (!kb_left && is_left) {
            control_stop_all(app);
            kb_left = is_left;
            move_left_start(app);
        }
        else if (!kb_up && is_up) {
            control_stop_all(app);
            kb_up = is_up;
            move_forward_start(app);
        }
        else if (!kb_right && is_right) {
            control_stop_all(app);
            kb_right = is_right;
            move_right_start(app);
        }
        else if (!kb_down && is_down) {
            control_stop_all(app);
            kb_down = is_down;
            move_backwards_start(app);
        }
    } else {
        if (kb_left) {
            kb_left = 0;
            move_left_stop(app);
        }
        if (kb_up) {
            kb_up = 0;
            move_forward_stop(app);
        }
        if (kb_right) {
            kb_right = 0;
            move_right_stop(app);
        }
        if (kb_down) {
            kb_down = 0;
            move_backwards_stop(app);
        }
    }
    return 0;
}

int control_vnc_key(struct app_state_t *app, int down, int key)
{
    int is_left = 0, is_up = 0, is_right = 0, is_down = 0;
    if (key == 65361) {
        is_left = 1;
    } else if (key == 65362) {
        is_up = 1;
    } else if (key == 65363) {
        is_right = 1;
    } else if (key == 65363) {
        is_down = 1;
    }
    if (!kb_left && is_left) {
        kb_left = is_left;
        move_left_start(app);
    } else if (kb_left && !is_left) {
        kb_left = is_left;
        move_left_stop(app);
    }
    if (!kb_up && is_up) {
        kb_up = is_up;
        move_forward_start(app);
    } else if (kb_up && !is_up) {
        kb_up = is_up;
        move_forward_stop(app);
    }
    if (!kb_right && is_right) {
        kb_right = is_right;
        move_right_start(app);
    } else if (kb_right && !is_right) {
        kb_right = is_right;
        move_right_stop(app);
    }
    if (!kb_down && is_down) {
        kb_down = is_down;
        move_backwards_start(app);
    } else if (kb_down && !is_down) {
        kb_down = is_down;
        move_backwards_stop(app);
    }
    return 0;
}

int control_destroy(struct app_state_t *app)
{
    int res = control_stop_all(app);
    return res;
}