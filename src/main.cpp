#include <windows.h>

#include "application/application.h"

int main()
{
    // Disable quick edit so the application loading cannot be accidentally paused
    HANDLE handle{GetStdHandle(STD_INPUT_HANDLE)};
    DWORD mode{};
    GetConsoleMode(handle, &mode);
    SetConsoleMode(handle, (mode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);

    Application application;
    application.run();

    // Restore any console settings
    SetConsoleMode(handle, mode | ENABLE_EXTENDED_FLAGS);
    return 0;
}
