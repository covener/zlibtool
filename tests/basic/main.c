#include <stdio.h>
#include <dlfcn.h>

int main() {
    void (*hello_func)();
    void *handle = dlopen("built/libhello.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    hello_func = dlsym(handle, "hello");
    if (dlerror() != NULL) {
        fprintf(stderr, "%s\n", dlerror());
        dlclose(handle);
        return 1;
    }
    hello_func();
    dlclose(handle);
    return 0;
}
