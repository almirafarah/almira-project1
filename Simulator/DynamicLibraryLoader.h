#pragma once

#include <string>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#define USING_WINDOWS_DYNAMIC_LOADING
#else
#include <dlfcn.h>
#define USING_UNIX_DYNAMIC_LOADING
#endif

/**
 * Cross-platform dynamic library loader for .so/.dll files
 */
class DynamicLibraryLoader {
public:
    DynamicLibraryLoader() = default;
    ~DynamicLibraryLoader();

    // Non-copyable
    DynamicLibraryLoader(const DynamicLibraryLoader&) = delete;
    DynamicLibraryLoader& operator=(const DynamicLibraryLoader&) = delete;

    /**
     * Load a dynamic library (.so on Linux, .dll on Windows)
     * @param library_path Path to the library file
     * @return true if successful, false otherwise
     */
    bool loadLibrary(const std::string& library_path);

    /**
     * Get a function pointer from the loaded library
     * @param function_name Name of the function to load
     * @return Function pointer, or nullptr if not found
     */
    void* getFunction(const std::string& function_name);

    /**
     * Check if a library is currently loaded
     */
    bool isLoaded() const;

    /**
     * Get the last error message
     */
    std::string getLastError() const;

    /**
     * Unload the library (called automatically in destructor)
     */
    void unload();

private:
#ifdef _WIN32
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
    std::string last_error_;
};
