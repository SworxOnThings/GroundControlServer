#ifndef DEBUG
#define DEBUG

#define DEBUG_PRINT(fmt, ...) \
do { \
    fprintf(stderr, "Debug: %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, \
    __VA_ARGS__); \
} while (0)

#endif