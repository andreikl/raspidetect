const char* cuda_get_error_message(CUresult error)
{
    const char* unknown_error = "UNKNOWN_ERROR";
    const char* error_str = NULL;
    CUresult res = cuGetErrorString(error, &error_str);
    if (res != CUDA_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to get error string, res %d\n%s:%s:%d\n", res, __FILE__, __FUNCTION__, __LINE__);
        return unknown_error;
    }
    if (error_str == NULL) {
        return unknown_error;
    }
    return error_str;
}
