# Arrowix OS - Shared freestanding build flags for kernel-space code.
#
# Provides helper functions to apply consistent compile/link options to the
# kernel, boot stub, drivers, fs, and libk targets.

# --- Common freestanding compile flags (C and C++) ---------------------------
set(ARROWIX_FREESTANDING_FLAGS
    -ffreestanding          # no hosted libc/runtime assumptions
    -fno-stack-protector    # no __stack_chk_guard from host
    -fno-stack-clash-protection
    -fno-pic -fno-pie       # kernel is loaded at a fixed address
    -mno-red-zone           # the red zone is unsafe with interrupts
    -mcmodel=kernel         # code lives in the top 2 GiB (higher-half)
    -mno-mmx -mno-sse -mno-sse2  # no FPU/SSE until enabled in the kernel
    -Wall -Wextra
)

# --- C-specific flags ---------------------------------------------------------
set(ARROWIX_C_FLAGS
    ${ARROWIX_FREESTANDING_FLAGS}
    -std=c17
)

# --- C++-specific flags (freestanding: no RTTI, no exceptions) ----------------
set(ARROWIX_CXX_FLAGS
    ${ARROWIX_FREESTANDING_FLAGS}
    -std=c++20
    -fno-exceptions
    -fno-rtti
    -fno-use-cxa-atexit
    -fno-threadsafe-statics
)

# --- Link flags for the final kernel image ------------------------------------
set(ARROWIX_KERNEL_LINK_FLAGS
    -nostdlib               # do not link the host C runtime
    -static
    -z max-page-size=0x1000 # keep sections page-aligned at 4 KiB
)

# Apply freestanding flags to a target based on its languages.
function(arrowix_apply_kernel_flags target)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${ARROWIX_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${ARROWIX_CXX_FLAGS}>
    )
    set_target_properties(${target} PROPERTIES
        C_STANDARD 17
        CXX_STANDARD 20
        C_STANDARD_REQUIRED ON
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE OFF
    )
endfunction()
