# Arrowix OS - Filesystems (VFS layer, ramfs, FAT32).
# Built as an OBJECT library linked into the final kernel image.

# Filesystem sources are added in Phase 6. A placeholder keeps the object
# library valid until then.
set(FS_SOURCES
    fs_placeholder.c
    # vfs/vfs.c       # Phase 6
    # ramfs/ramfs.c   # Phase 6
    # fat32/fat32.c   # Phase 6
)

add_library(arrowix_fs OBJECT ${FS_SOURCES})

target_include_directories(arrowix_fs PUBLIC ${ARROWIX_KERNEL_INCLUDE_DIRS})
arrowix_apply_kernel_flags(arrowix_fs)
