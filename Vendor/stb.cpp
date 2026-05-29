#include "../src/decl/Memory.hpp"

#define STBI_MALLOC(s) Memory::Alloc(s, MEMCAT(Texture))
#define STBI_REALLOC(p, s) Memory::ReAlloc(p, s, MEMCAT(Texture))
#define STBI_FREE(p) Memory::Free(p)

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb/stb_image.h>
