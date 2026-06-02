; Arrowix OS - Initial page tables for the long-mode transition.
;
; Builds 4-level paging tables with 2 MiB huge pages, mapping the first 1 GiB
; of physical memory twice:
;   * Identity   : virtual 0x0..0x40000000          (needed right after PG=1)
;   * Higher half: virtual 0xFFFFFFFF80000000..+1GiB (where the kernel is linked)
;
; The same PD (p2_table) is shared by both mappings. See docs/architecture/paging.md.
;
; Index derivation for 0xFFFFFFFF80000000: PML4[511] -> PDPT[510] -> PD[0].

bits 32

PRESENT   equ 1 << 0
WRITABLE  equ 1 << 1
HUGE_PAGE equ 1 << 7

global setup_page_tables
global p4_table

section .boot.text
setup_page_tables:
    ; Zero the four tables (4 * 4096 bytes) so unused entries are non-present.
    mov edi, p4_table
    xor eax, eax
    mov ecx, (4 * 4096) / 4        ; dword count
    rep stosd

    ; PML4[0] -> PDPT_low (identity), PML4[511] -> PDPT_high (higher half)
    mov eax, p3_low
    or eax, PRESENT | WRITABLE
    mov [p4_table + 0 * 8], eax

    mov eax, p3_high
    or eax, PRESENT | WRITABLE
    mov [p4_table + 511 * 8], eax

    ; PDPT_low[0] -> PD ; PDPT_high[510] -> PD (same PD shared)
    mov eax, p2_table
    or eax, PRESENT | WRITABLE
    mov [p3_low + 0 * 8], eax

    mov eax, p2_table
    or eax, PRESENT | WRITABLE
    mov [p3_high + 510 * 8], eax

    ; Fill PD with 512 huge pages of 2 MiB => maps physical 0..1 GiB.
    xor ecx, ecx
.map_pd:
    mov eax, 0x200000              ; 2 MiB
    mul ecx                        ; eax = ecx * 2 MiB (edx = 0 for ecx < 2048)
    or eax, PRESENT | WRITABLE | HUGE_PAGE
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd
    ret

section .boot.bss
align 4096
p4_table:  resb 4096               ; PML4
p3_low:    resb 4096               ; PDPT (identity)
p3_high:   resb 4096               ; PDPT (higher half)
p2_table:  resb 4096               ; PD (shared, 2 MiB pages)
