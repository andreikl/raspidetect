#ifndef utils_h
#define utils_h

#include "main.h"

#define MASK1_565   (0x0000FFFF)
#define MASK2_565   (0xFFFF0000)

#define R_888_MASK      (0x00FF0000)
#define G_888_MASK      (0x0000FF00)
#define B_888_MASK      (0x000000FF)
#define ALPHA_888_MASK  (0xFF000000)

#define GET_A(argb) ((argb >> 24) & 0xff)
#define GET_R(argb) ((argb >> 16) & 0xff)
#define GET_G(argb) ((argb >> 8) & 0xff)
#define GET_B(argb) (argb & 0xff)

                   // (0b00000000 00000000 11111000 00000000)
#define R1_565_MASK      (0b00000000000000001111100000000000)
                   // (0b00000000 00000000 00000111 11100000)
#define G1_565_MASK      (0b00000000000000000000011111100000)
                   // (0b00000000 00000000 00000000 00011111)
#define B1_565_MASK      (0b00000000000000000000000000011111)
                   // (0b11111000 00000000 00000000 00000000)
#define R2_565_MASK      (0b11111000000000000000000000000000)
                   // (0b00000111 11100000 00000000 00000000)
#define G2_565_MASK      (0b00000111111000000000000000000000)
                   // (0b00000000 00011111 00000000 00000000)
#define B2_565_MASK      (0b00000000000111110000000000000000)

#define GET_R5651(rgb565) ( ((rgb565) & R1_565_MASK) >> 11 )
#define GET_G5651(rgb565) ( ((rgb565) & G1_565_MASK) >> 5 )
#define GET_B5651(rgb565) ( ((rgb565) & B1_565_MASK))
#define GET_R5652(rgb565) ( ((rgb565) & R2_565_MASK) >> 27 )
#define GET_G5652(rgb565) ( ((rgb565) & G2_565_MASK) >> 21 )
#define GET_B5652(rgb565) ( ((rgb565) & B2_565_MASK) >> 16)

#define SET_R5651(rgb565) ( ((rgb565) & 0b11111) << 11 )
#define SET_G5651(rgb565) ( ((rgb565) & 0b111111) << 5 )
#define SET_B5651(rgb565) ( ((rgb565) & 0b11111))
#define SET_R5652(rgb565) ( ((rgb565) & 0b11111) << 27 )
#define SET_G5652(rgb565) ( ((rgb565) & 0b111111) << 21 )
#define SET_B5652(rgb565) ( ((rgb565) & 0b11111) << 16 )

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)

void utils_parse_args(int argc, char** argv);
const char *utils_read_str_value(const char name[], char *def_value);
int utils_read_int_value(const char name[], int def_value);

int utils_fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read);
void *utils_read_file(const char *path, size_t *len);
int utils_write_file(const char *path, const uint8_t *data, int len);

void utils_get_cpu_load(char * buffer, struct cpu_state_t *cpu);
void utils_get_memory_load(char * buffer, struct memory_state_t *memory);
void utils_get_temperature(char * buffer, struct temperature_state_t *temperature);

int utils_get_worker_buffer();

#endif //utils_h
