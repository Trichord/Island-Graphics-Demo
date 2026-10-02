#include <windows.h>
#include <filesystem>

#include "application/application.h"

int main(int argc, char *argv[])
{
    // Disable quick edit so the application loading cannot be accidentally paused
    HANDLE handle{GetStdHandle(STD_INPUT_HANDLE)};
    DWORD mode{};
    GetConsoleMode(handle, &mode);
    SetConsoleMode(handle, (mode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);

    // Fix all relative filepaths if run from terminal outside .exe folder
    std::filesystem::current_path(std::filesystem::absolute(argv[0]).parent_path());

    Application application;
    application.run();

    // Restore any console settings
    SetConsoleMode(handle, mode | ENABLE_EXTENDED_FLAGS);
    return 0;
}
