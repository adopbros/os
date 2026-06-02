# Arrowix OS - Helper to build a bootable GRUB ISO from the kernel ELF.
#
# Defines the `iso` target which stages the kernel + grub.cfg into an isodir and
# runs grub-mkrescue (requires grub-mkrescue + xorriso on PATH).

find_program(GRUB_MKRESCUE NAMES grub-mkrescue grub2-mkrescue)

function(arrowix_add_iso_target kernel_target)
    set(_isodir "${CMAKE_BINARY_DIR}/isodir")
    set(_iso "${CMAKE_BINARY_DIR}/arrowix.iso")
    set(_grubcfg "${CMAKE_SOURCE_DIR}/boot/grub/grub.cfg")

    if(NOT GRUB_MKRESCUE)
        message(WARNING
            "grub-mkrescue not found: the 'iso' target will be unavailable. "
            "Install grub + xorriso (recommended under WSL2/Linux).")
        return()
    endif()

    add_custom_target(iso
        DEPENDS ${kernel_target}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_isodir}/boot/grub"
        COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:${kernel_target}>" "${_isodir}/boot/arrowix.elf"
        COMMAND ${CMAKE_COMMAND} -E copy "${_grubcfg}" "${_isodir}/boot/grub/grub.cfg"
        COMMAND ${GRUB_MKRESCUE} -o "${_iso}" "${_isodir}"
        COMMENT "Creating bootable ISO: ${_iso}"
        VERBATIM
    )
endfunction()
