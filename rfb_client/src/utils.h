void utils_parse_args(int argc, char** argv);

void utils_default_status(struct app_state_t *state);

char *utils_read_str_value(const char name[], char *def_value);
int utils_read_int_value(const char name[], int def_value);

int utils_fill_buffer(const char *path, uint8_t *buffer, int buffer_size, size_t *read);

int find_nal(uint8_t *buf, int buf_size, int *nal_start, int *nal_end);

char* convert_general_error(int error);
char* convert_hresult_error(HRESULT err);