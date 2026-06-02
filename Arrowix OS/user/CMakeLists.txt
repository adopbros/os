# Arrowix OS - User space: init (PID 1), shell, apps, and the GUI stack.
#
# Built only when ARROWIX_BUILD_USERSPACE is ON (see root CMakeLists.txt).
# User-space programs link against the freestanding libc (arrowix_c) and a
# dedicated linker script (linker/user.ld). Components land in Phases 7-8.

message(STATUS "Arrowix user space: components are introduced in Phase 7+ (init, shell, gui).")

# add_subdirectory(init)    # Phase 7 - PID 1
# add_subdirectory(shell)   # Phase 7
# add_subdirectory(apps)    # Phase 7+
# add_subdirectory(gui)     # Phase 8
