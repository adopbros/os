# Arrowix OS - Device drivers (video, input, storage, serial, timer, bus).
# Built as an OBJECT library linked into the final kernel image.

# Driver sources are added as Phase 2/5 land.
set(DRIVER_SOURCES
    timer/pit.c             # Phase 2 - kernel heartbeat
    input/keyboard_ps2.c    # PS/2 keyboard (IRQ1)
    # serial/uart16550.c    # Phase 5 (serial currently lives in core/console.c)
    # video/vga_text.c      # Phase 5
    # video/vesafb.c        # Phase 5
    # bus/pci.c             # Phase 5
    # storage/ata_pio.c     # Phase 5
)

add_library(arrowix_drivers OBJECT ${DRIVER_SOURCES})

target_include_directories(arrowix_drivers PUBLIC ${ARROWIX_KERNEL_INCLUDE_DIRS})
arrowix_apply_kernel_flags(arrowix_drivers)
