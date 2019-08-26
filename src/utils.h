#ifndef utils_h
#define utils_h

#include "main.h"

void parse_args(int argc, char** argv);
void print_help(void);

void default_status(APP_STATE *state);

void default_signal_handler(int signal_number);

char *read_str_value(const char name[], char *def_value);
int read_int_value(const char name[], int def_value);

int fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read);
//unsigned char *read_file(const char path[], int *size);
//void write_file(const char *path, unsigned char *data, int width, int height);

void get_cpu_load(char * buffer, CPU_STATE *state);
void get_memory_load(char * buffer, MEMORY_STATE *state);
void get_temperature(char * buffer, TEMPERATURE_STATE *state);

#endif //utils_h