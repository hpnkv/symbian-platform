#include <cstring>

using MemmoveFunction = void* (*)(void*, const void*, size_t);

// Keep the imported function's address in writable storage so the image must
// relocate a real PLT pointer before the first call. The caller is in a
// separate translation unit and reaches this data through the local GOT.
MemmoveFunction RuntimeImportedMemmove = &memmove;
