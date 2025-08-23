#include "DynamicLibraryLoader.h"
#include <iostream>

// Avoid explicitly unloading libraries in the destructor. Some of the
// dynamically loaded modules register global objects whose destructors may
// run after the Simulator shuts down. Calling dlclose/FreeLibrary here could
// invalidate code that these destructors rely on and lead to crashes at
// program termination.
DynamicLibraryLoader::~DynamicLibraryLoader() {
    // Intentionally left empty; rely on the OS to unload libraries at exit.
}

bool DynamicLibraryLoader::loadLibrary(const std::string& library_path) {
    // Unload any existing library first
    unload();

#ifdef _WIN32
    // Windows implementation
    handle_ = LoadLibraryA(library_path.c_str());
    if (!handle_) {
        DWORD error = GetLastError();
        last_error_ = "Failed to load library: " + library_path + 
                     " (Windows error code: " + std::to_string(error) + ")";
        return false;
    }
#else
    // Linux/Unix implementation
    handle_ = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!handle_) {
        last_error_ = "Failed to load library: " + library_path + 
                     " (dlopen error: " + std::string(dlerror()) + ")";
        return false;
    }
#endif

    last_error_.clear();
    return true;
}

void* DynamicLibraryLoader::getFunction(const std::string& function_name) {
    if (!isLoaded()) {
        last_error_ = "No library loaded";
        return nullptr;
    }

#ifdef _WIN32
    // Windows implementation
    FARPROC proc = GetProcAddress(handle_, function_name.c_str());
    if (!proc) {
        DWORD error = GetLastError();
        last_error_ = "Failed to find function: " + function_name + 
                     " (Windows error code: " + std::to_string(error) + ")";
        return nullptr;
    }
    return reinterpret_cast<void*>(proc);
#else
    // Linux/Unix implementation
    void* func = dlsym(handle_, function_name.c_str());
    const char* error = dlerror();
    if (error) {
        last_error_ = "Failed to find function: " + function_name + 
                     " (dlsym error: " + std::string(error) + ")";
        return nullptr;
    }
    return func;
#endif
}

bool DynamicLibraryLoader::isLoaded() const {
    return handle_ != nullptr;
}

std::string DynamicLibraryLoader::getLastError() const {
    return last_error_;
}

void DynamicLibraryLoader::unload() {
    if (handle_) {
#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}
