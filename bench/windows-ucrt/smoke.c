#include <windows.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static void *worker(void *input) {
    *(int *)input = 73;
    return NULL;
}

int main(void) {
    int value = 0;
    pthread_t thread;
    char path[MAX_PATH];
    HMODULE crt = GetModuleHandleA("ucrtbase.dll");
    if (!crt || !GetModuleFileNameA(crt, path, sizeof(path)) || sizeof(void *) != 8) return 1;
    if (pthread_create(&thread, NULL, worker, &value) || pthread_join(thread, NULL)) return 2;
    printf("UCRT runtime=%s pointer_bits=%u winpthreads_result=%d\n", path,
           (unsigned)(8 * sizeof(void *)), value);
    return value == 73 ? 0 : 3;
}
