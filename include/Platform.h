#pragma once

namespace ndt::platform {

bool guardedDestroy(void (*dtor)(void*, int), void* session);

void pinSelfModule();

} // namespace ndt::platform
