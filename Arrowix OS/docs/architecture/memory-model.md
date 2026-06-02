# Arrowix OS - Modelo de Memoria

Define el reparto del espacio de direcciones virtuales de 64 bits y la organizacion
de la memoria fisica.

## Espacio de direcciones virtual (canonico de 48 bits)

x86_64 (sin LA57) usa direcciones canonicas de 48 bits: la mitad baja
`0x0000000000000000 .. 0x00007FFFFFFFFFFF` y la mitad alta
`0xFFFF800000000000 .. 0xFFFFFFFFFFFFFFFF`. El rango intermedio no es canonico.

```text
0x0000000000000000  +-------------------------------+
                    |  Espacio de USUARIO (Ring 3)  |  codigo, datos, heap, pila,
                    |                               |  mmap de cada proceso
0x00007FFFFFFFFFFF  +-------------------------------+
                    |        (no canonico)          |
0xFFFF800000000000  +-------------------------------+
                    |  Direct map de RAM fisica     |  (Fase 3) acceso del kernel a
                    |                               |  cualquier marco fisico
                    +-------------------------------+
                    |  Heap del kernel / vmalloc    |
                    +-------------------------------+
0xFFFFFFFF80000000  +-------------------------------+
                    |  Imagen del KERNEL (-2 GiB)   |  .text/.rodata/.data/.bss
                    |  (higher-half, mcmodel=kernel)|
0xFFFFFFFFFFFFFFFF  +-------------------------------+
```

- **Kernel higher-half:** enlazado en `0xFFFFFFFF80000000` (ver `linker/kernel.ld`).
  Compilado con `-mcmodel=kernel` (el codigo asume los altos 2 GiB).
- **Direct map:** region alta donde toda la RAM fisica esta mapeada linealmente, para
  que el kernel convierta `phys <-> virt` con un simple offset.
- **Usuario:** cada proceso tiene su propio PML4; la mitad baja es privada, la mitad
  alta (kernel) se comparte mapeando las mismas tablas de nivel superior.

## Memoria fisica

Origen de la verdad: el **tag de mapa de memoria de Multiboot2** (`mmap`), que enumera
regiones disponibles/reservadas. A partir de el:

- **PMM (`kernel/mm/pmm.c`):** administra marcos de 4 KiB.
  - Implementacion inicial: **bitmap** (1 bit por marco).
  - Evolucion: **buddy allocator** para asignaciones contiguas de varios marcos.
  - Bootstrap: las primeras estructuras del PMM se ubican en RAM libre conocida antes
    de existir un heap.
- Regiones reservadas: imagen del kernel, tablas de paginas de boot, framebuffer,
  modulos (initrd) y zonas marcadas por el firmware.

## Heap del kernel

`kernel/mm/kheap.c` ofrece `kmalloc`/`kfree`:

- Solicita paginas al VMM/PMM y las subdivide.
- Estrategia inicial: lista libre con cabeceras de bloque; evolucion a **slab/slub**
  para objetos de tamano fijo y menor fragmentacion.

## Memoria de usuario (Fase 7)

- Cada proceso: PML4 propio; `fork` con **copy-on-write** sobre fallos de pagina.
- Pila de usuario y heap (`brk`/`mmap`) en la mitad baja.
- Validacion estricta de punteros de usuario en la frontera de syscalls.

## Proteccion

- **NX** (No-Execute) en paginas de datos (requiere `EFER.NXE`).
- **SMEP/SMAP** (Fase avanzada): impedir que el kernel ejecute/acceda memoria de
  usuario inadvertidamente.
- **KASLR** (futuro): aleatorizar la base del kernel/direct map.

## Resumen de constantes (propuestas)

| Constante              | Valor                  | Significado |
|------------------------|------------------------|-------------|
| `KERNEL_VMA`           | `0xFFFFFFFF80000000`   | Base virtual del kernel |
| `PAGE_SIZE`            | `4096`                 | Tamano de pagina base |
| `HUGE_2M`              | `0x200000`             | Pagina grande de 2 MiB |
| `DIRECT_MAP_BASE`      | `0xFFFF800000000000`   | Base del direct map (propuesta) |

Estas constantes se definiran en `kernel/include/arrowix/` y deben mantenerse
coherentes con `linker/kernel.ld` y `boot/paging_boot.asm`.
