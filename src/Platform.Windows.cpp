#include "Platform.h"

#include <windows.h>

namespace ndt::platform {

bool guardedDestroy(void (*dtor)(void*, int), void* session) {
    if (!dtor || !session)
        return false;
    __try {
        dtor(session, 1);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void pinSelfModule() {
    HMODULE self = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                         reinterpret_cast<LPCWSTR>(&pinSelfModule),
                         &self);
}

} // namespace ndt::platform
