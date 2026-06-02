# Arrowix OS - Plan de Desarrollo Tecnico

> Sistema operativo de arquitectura hibrida, 64 bits (x86_64), modular y robusto.
> Arranque GRUB/Multiboot2 -> Long Mode -> kernel higher-half.
> Build con CMake + cross-compiler `x86_64-elf`. Lenguajes: C17, C++ freestanding, NASM.

Este documento es el mapa de ruta tecnico. Cada **fase** define su alcance, su
**hito verificable** (criterio objetivo de "hecho") y los **desafios de bajo nivel**
que debemos resolver. Las fases son incrementales: cada una asume las anteriores.

---

## Vision y principios de diseno

- **Hibrido:** nucleo monolitico en su rendimiento, pero con subsistemas desacoplados
  (drivers, fs) detras de interfaces claras, con la opcion de migrar componentes a
  espacio de usuario mas adelante.
- **Higher-half kernel:** el kernel reside en `0xFFFFFFFF80000000` (-2 GiB), dejando
  la mitad baja del espacio de direcciones para los procesos de usuario.
- **Portabilidad por capas:** todo lo dependiente de la arquitectura vive bajo
  `arch/x86_64/`; el resto del kernel es agnostico.
- **Arranque temprano observable:** salida por puerto serie (COM1) desde el primer
  instante para depurar sin pantalla.
- **Sin dependencias del host:** compilacion freestanding, sin la libc del sistema.

---

## Resumen de fases

| Fase | Nombre                      | Hito principal |
|------|-----------------------------|----------------|
| 0    | Toolchain e Infraestructura | ISO arrancable vacio compila |
| 1    | Arranque y Long Mode        | `kmain` corre en 64-bit e imprime |
| 2    | Arquitectura Core (x86_64)  | Excepciones, IRQs y timer funcionan |
| 3    | Gestion de Memoria          | `kmalloc`/`kfree` y paginacion dinamica |
| 4    | Multitarea y Scheduling     | Hilos de kernel concurrentes |
| 5    | Drivers                     | Teclado + lectura de disco |
| 6    | Sistema de Archivos         | Montar y leer archivos (FAT32) |
| 7    | Espacio de Usuario          | Ejecutar un programa Ring 3 + shell |
| 8    | Interfaz Grafica (GUI)      | Escritorio compuesto sobre framebuffer |

---

## Fase 0 - Toolchain e Infraestructura

**Objetivo:** poder compilar y arrancar un artefacto, aunque no haga nada util.

**Alcance**
- Construir `x86_64-elf-binutils` + `x86_64-elf-gcc` (freestanding) con
  `toolchain/build-cross-gcc.sh`.
- Toolchain file de CMake (`cmake/toolchain-x86_64-elf.cmake`) + flags compartidos
  (`cmake/KernelFlags.cmake`).
- Instalar `nasm`, `qemu-system-x86_64`, `grub-mkrescue`/`xorriso`, `gdb`.
- Scripts: `scripts/make-iso.sh` (grub-mkrescue), `scripts/run-qemu.sh`,
  `scripts/debug-gdb.sh`.
- Esqueleto de CI (compilar + bootear en QEMU headless con timeout).

**Hito**
- `cmake --build build` produce `arrowix.elf`; `make-iso.sh` produce `arrowix.iso`;
  QEMU arranca el ISO sin error de GRUB.

**Desafios de bajo nivel**
- Configurar GCC como cross-compiler real (`--without-headers`, `--with-newlib` no,
  target `x86_64-elf`) y evitar contaminacion con la libc del host.
- En Windows: orquestar todo via WSL2/MSYS2 de forma reproducible.
- `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` para que CMake no intente enlazar
  un ejecutable hosted durante la deteccion del compilador.

---

## Fase 1 - Arranque y Transicion a Long Mode

**Objetivo:** llevar la CPU de Modo Protegido (32-bit, entregado por GRUB) a Long
Mode (64-bit) y saltar a C/C++.

**Alcance**
- `boot/multiboot2_header.asm`: cabecera Multiboot2 (magic, arquitectura, checksum,
  tags de framebuffer y de fin).
- `boot/boot32.asm`: punto de entrada de 32 bits; validar `EAX == 0x36d76289`,
  guardar puntero a la info Multiboot2 (`EBX`), montar pila temporal.
- Chequeos de capacidad: CPUID disponible, funciones extendidas, bit LM
  (`CPUID 0x80000001`, `EDX` bit 29).
- `boot/paging_boot.asm`: construir PML4/PDPT/PD iniciales (identidad de 1 GiB con
  paginas de 2 MiB + mapeo higher-half).
- `boot/long_mode.asm`: secuencia PAE -> EFER.LME -> CR0.PG; cargar GDT64; far jump
  a segmento de codigo de 64 bits.
- `boot/gdt64.asm`: GDT plana de 64 bits (codigo/datos kernel).
- `kernel/core/kmain.c`: primera funcion C de 64 bits; salida por serie/VGA.

**Hito**
- En QEMU, el kernel imprime un mensaje de bienvenida por COM1 y por VGA desde
  `kmain`, ejecutando en Long Mode (verificable con `info registers` en QEMU/GDB:
  `CR0.PG=1`, `EFER.LMA=1`).

**Desafios de bajo nivel**
- Orden exacto y atomico de habilitacion (PAE antes de LME antes de PG).
- Page tables alineadas a 4 KiB y referenciadas por direccion fisica en `CR3`.
- Coherencia entre el identity map (necesario en el instante de activar paginacion)
  y el higher-half (donde se enlaza el kernel): el far jump debe aterrizar en una
  direccion valida en ambos esquemas.
- No usar la red-zone ni instrucciones SSE antes de habilitarlas (`-mno-red-zone`,
  configurar `CR0`/`CR4` para FPU/SSE si se usan).

---

## Fase 2 - Arquitectura Core (x86_64)

**Objetivo:** manejar interrupciones y excepciones de forma robusta.

**Alcance**
- `arch/x86_64/gdt.c` + TSS (necesaria para IST y futuros cambios de privilegio).
- `arch/x86_64/idt.c`: 256 entradas; stubs en ensamblador que apilan un marco comun.
- ISRs para las 32 excepciones de CPU; handlers de IRQ.
- Remapeo del PIC 8259 y/o configuracion de Local APIC + IO APIC.
- Driver serie (`drivers/serial/uart16550.c`) y `printk`/`panic` tempranos.
- Timer: PIT (`drivers/timer/pit.c`) y/o LAPIC timer para el tick del sistema.

**Hito**
- Una excepcion provocada (p. ej. `#DE` division por cero) es capturada y reportada
  por `panic` con volcado de registros; el timer genera ticks periodicos contados.

**Desafios de bajo nivel**
- Formato del marco de interrupcion en x86_64 (alineacion de pila a 16 bytes,
  codigo de error presente solo en ciertas excepciones).
- IST/TSS para excepciones criticas (doble fallo) con pilas dedicadas.
- Coordinar EOI correctamente (PIC vs APIC) para no perder/duplicar interrupciones.

---

## Fase 3 - Gestion de Memoria

**Objetivo:** administrar memoria fisica y virtual y ofrecer asignacion dinamica.

**Alcance**
- Parsear el mapa de memoria del tag Multiboot2 (`mmap`).
- `mm/pmm.c`: gestor de marcos fisicos (bitmap inicialmente; buddy mas adelante).
- `mm/vmm.c`: gestion de paginacion de 4 niveles (`map`, `unmap`, `protect`),
  reconstruccion de tablas definitivas y descarte del identity map de boot.
- `mm/kheap.c`: heap del kernel (`kmalloc`/`kfree`, p. ej. lista libre + slabs).
- Region de mapeo directo (direct map) de la RAM fisica para acceso del kernel.

**Hito**
- `kmalloc`/`kfree` superan una bateria de pruebas (asignaciones aleatorias,
  fragmentacion) sin corromper memoria; `map`/`unmap` verificados con fallos de
  pagina controlados.

**Desafios de bajo nivel**
- Estrategia de acceso a tablas de paginas: recursive mapping vs direct map.
- Invalidacion de TLB (`invlpg`) y futura coordinacion multinucleo (TLB shootdown).
- Bootstrap del propio PMM: asignar las primeras estructuras antes de tener heap.
- Manejo de paginas de 2 MiB/1 GiB vs 4 KiB y alineaciones.

---

## Fase 4 - Multitarea y Scheduling

**Objetivo:** ejecutar multiples hilos de kernel de forma concurrente y preemptiva.

**Alcance**
- `sched/thread.c`: estructura de hilo/contexto (registros, pila kernel, estado).
- `arch/x86_64/context_switch.asm`: guardado/restauracion de contexto.
- `sched/scheduler.c`: round-robin -> colas por prioridad; tick del timer dispara
  la planificacion.
- `sync/`: spinlocks y mutexes; deshabilitacion de interrupciones donde aplique.

**Hito**
- Varios hilos de kernel se intercalan de forma justa bajo preempcion por timer;
  los primitivos de sincronizacion evitan condiciones de carrera observables.

**Desafios de bajo nivel**
- Guardar/restaurar el estado completo (incluido el puntero de pila y, si se usa,
  el estado FPU/SSE de forma perezosa).
- Preempcion segura: no cambiar de contexto con un spinlock tomado de forma insegura.
- Idle thread y manejo de la pila por-CPU.

---

## Fase 5 - Drivers

**Objetivo:** interactuar con hardware real: video, entrada y almacenamiento.

**Alcance**
- Video: `drivers/video/vga_text.c` (modo texto) y `drivers/video/vesafb.c`
  (framebuffer lineal del tag Multiboot2).
- Entrada: `drivers/input/keyboard_ps2.c` (IRQ1, scancodes -> eventos).
- Bus: `drivers/bus/pci.c` (enumeracion por config space).
- Almacenamiento: `drivers/storage/ata_pio.c` y luego `ahci.c` (SATA via PCI).
- Modelo de driver minimo: registro, IRQ handlers, colas de E/S.

**Hito**
- Se leen sectores de un disco virtual de QEMU y el contenido se valida; las
  pulsaciones de teclado se traducen a caracteres y se muestran en pantalla.

**Desafios de bajo nivel**
- MMIO vs PIO; barreras de memoria y `volatile` para registros de dispositivo.
- DMA y descriptores para AHCI; coherencia de cache.
- Comparticion y enrutamiento de IRQs (especialmente con APIC).

---

## Fase 6 - Sistema de Archivos

**Objetivo:** abstraer almacenamiento en archivos y directorios.

**Alcance**
- `fs/vfs/`: capa VFS (vnodes, operaciones, puntos de montaje, tabla de descriptores).
- `fs/ramfs/`: sistema en RAM para initrd / archivos temporales.
- `fs/fat32/`: lectura (y luego escritura) de FAT32 sobre el driver de bloques.
- Cache de bloques unificada.

**Hito**
- Se monta una imagen FAT32, se resuelve una ruta y se lee un archivo completo a
  traves de la API del VFS.

**Desafios de bajo nivel**
- Cache de bloques con politica de reemplazo y consistencia (write-back/through).
- Resolucion de rutas, montajes anidados y manejo de nombres FAT (8.3 / LFN).
- Concurrencia: bloqueo de inodos/vnodes.

---

## Fase 7 - Espacio de Usuario

**Objetivo:** ejecutar programas en Ring 3 aislados con llamadas al sistema.

**Alcance**
- Configurar Ring 3: segmentos de usuario en GDT, TSS con `RSP0`.
- `syscall`/`sysret` (MSRs `STAR`, `LSTAR`, `SFMASK`); tabla de syscalls.
- Cargador ELF de ejecutables de usuario; creacion de espacios de direcciones.
- Modelo de procesos: `fork`/`exec`/`exit`/`wait` (o equivalentes), copia de paginas.
- Port de `libc/` (subconjunto freestanding) + crt0; `user/init` (PID 1) y `user/shell`.

**Hito**
- `init` arranca en Ring 3, lanza una shell que ejecuta un programa de usuario que
  hace syscalls (escribir en consola, leer teclado) y termina limpiamente.

**Desafios de bajo nivel**
- Separacion estricta kernel/usuario; validacion de punteros provenientes de usuario.
- Cambio de pila en la entrada de syscall y manejo seguro de interrupciones anidadas.
- Copy-on-write y manejo de fallos de pagina para `fork`.

---

## Fase 8 - Interfaz Grafica (GUI)

**Objetivo:** un entorno grafico basico sobre el framebuffer.

**Alcance**
- `user/gui/compositor/`: composicion de superficies sobre el framebuffer (doble
  buffer).
- `user/gui/wm/`: gestor de ventanas (decoracion, foco, z-order).
- `user/gui/toolkit/`: widgets basicos (boton, label, ventana).
- Canal de eventos de entrada (teclado/raton) desde el kernel hacia las apps.

**Hito**
- Un escritorio muestra al menos dos ventanas movibles con widgets, redibujadas sin
  parpadeo, respondiendo a teclado y raton.

**Desafios de bajo nivel**
- Doble buffering y sincronizacion de cuadro para evitar tearing/parpadeo.
- Protocolo de eventos y memoria compartida entre compositor y clientes.
- Rendimiento del blitting en CPU (sin GPU) y formatos de pixel del framebuffer.

---

## Riesgos transversales y deuda tecnica a vigilar

- **Reproducibilidad del entorno** (toolchain identica entre desarrolladores/CI).
- **Depuracion temprana**: mantener salida serie y soporte GDB desde la Fase 1.
- **SMP (multinucleo)**: disenar estructuras pensando en por-CPU aunque arranquemos
  uniprocesador; AP startup y TLB shootdown llegaran despues.
- **Seguridad de memoria**: considerar SMEP/SMAP, NX y KASLR en fases avanzadas.
- **Pruebas**: bateria de pruebas unitarias para algoritmos (PMM, heap, FAT) en host
  cuando sea posible, y pruebas de humo en QEMU en CI.

---

## Convenciones

- Codigo dependiente de arquitectura: solo bajo `arch/x86_64/`.
- Estilo segun `.clang-format`; cabeceras publicas del kernel en
  `kernel/include/arrowix/`.
- Cada subsistema expone su API por cabeceras; nada de incluir `.c` directamente.
- Decisiones arquitectonicas relevantes se documentan como ADR en `docs/adr/`.
