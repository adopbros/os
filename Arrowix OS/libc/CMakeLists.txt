# Arrowix OS - Minimal freestanding C standard library (user space).
# Also provides a kernel subset (libk) compiled with kernel flags.
#
# Built only when ARROWIX_BUILD_USERSPACE is ON (see root CMakeLists.txt).

# libc sources are introduced in Phase 7. A placeholder keeps the static
# library valid until then.
set(LIBC_SOURCES
    libc_placeholder.c
    # string/memcpy.c   # Phase 7
    # string/memset.c   # Phase 7
    # string/strlen.c   # Phase 7
    # stdio/printf.c    # Phase 7
    # stdlib/malloc.c   # Phase 7
)

add_library(arrowix_c STATIC ${LIBC_SOURCES})
target_include_directories(arrowix_c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
