#ifndef ARCH_ELF_HH
#define ARCH_ELF_HH

enum {
    R_X86_64_NONE = 0, //  none none
    R_X86_64_64 = 1, //  word64 S + A
    R_X86_64_PC32 = 2, //  word32 S + A - P
    R_X86_64_GOT32 = 3, //  word32 G + A
    R_X86_64_PLT32 = 4, //  word32 L + A - P
    R_X86_64_COPY = 5, //  none none
    R_X86_64_GLOB_DAT = 6, //  word64 S
    R_X86_64_JUMP_SLOT = 7, //  word64 S
    R_X86_64_RELATIVE = 8, //  word64 B + A
    R_X86_64_GOTPCREL = 9, //  word32 G + GOT + A - P
    R_X86_64_32 = 10, //  word32 S + A
    R_X86_64_32S = 11, //  word32 S + A
    R_X86_64_16 = 12, //  word16 S + A
    R_X86_64_PC16 = 13, //  word16 S + A - P
    R_X86_64_8 = 14, //  word8 S + A
    R_X86_64_PC8 = 15, //  word8 S + A - P
    R_X86_64_DTPMOD64 = 16, //  word64
    R_X86_64_DTPOFF64 = 17, //  word64
    R_X86_64_TPOFF64 = 18, //  word64
    R_X86_64_TLSGD = 19, //  word32
    R_X86_64_TLSLD = 20, //  word32
    R_X86_64_DTPOFF32 = 21, //  word32
    R_X86_64_GOTTPOFF = 22, //  word32
    R_X86_64_TPOFF32 = 23, //  word32
    R_X86_64_PC64 = 24, //  word64 S + A - P
    R_X86_64_GOTOFF64 = 25, //  word64 S + A - GOT
    R_X86_64_GOTPC32 = 26, //  word32 GOT + A - P
    R_X86_64_SIZE32 = 32, //  word32 Z + A
    R_X86_64_SIZE64 = 33, //  word64 Z + A
    R_X86_64_TLSDESC = 36,
    R_X86_64_IRELATIVE = 37, //  word64 indirect(B + A)
};

/* for pltgot relocation */
#define ARCH_JUMP_SLOT R_X86_64_JUMP_SLOT
#define ARCH_TLSDESC R_X86_64_TLSDESC

#define ELF_KERNEL_MACHINE_TYPE 62

#define _AT_RANDOM       25
#define _AT_SYSINFO_EHDR 33
#define _AT_PAGESZ       6
#define _AT_NULL 0

struct auxv_t
{
  int a_type;
  union {
    long a_val;
    void *a_ptr;
    void (*a_fnc)();
  } a_un;
};

inline void elf_entry_point(void* ep, int argc, char** argv, u64 *random_bytes, uint64_t vdso_base, uint64_t page_size)
{
    struct auxv_t auxv_r;
    auxv_r.a_type = _AT_RANDOM;
    auxv_r.a_un.a_val = reinterpret_cast<uint64_t>(random_bytes);

    struct auxv_t auxv_v;
    auxv_v.a_type = _AT_SYSINFO_EHDR;
    auxv_v.a_un.a_val = vdso_base;

    struct auxv_t auxv_p;
    auxv_p.a_type = _AT_PAGESZ;
    auxv_p.a_un.a_val = page_size;

    u64 _argc = argc;

    //FIXME: Needs to be 16 bytes aligned
    asm volatile (
        "pushq $0\n\t" // Padding
        "pushq $0\n\t" // Zero AUX
        "pushq $0\n\t" // Zero AUX
        "pushq %2\n\t" // AT_RANDOM AUX
        "pushq %1\n\t" // AT_RANDOM AUX
        "pushq %4\n\t" // AT_SYSINFO_EHDR AUX
        "pushq %3\n\t" // AT_SYSINFO_EHDR AUX
        "pushq %6\n\t" // AT_PAGESZ AUX
        "pushq %5\n\t" // AT_PAGESZ AUX
        "pushq $0\n\t" // Zero
        "pushq %0\n\t" // End of environment pointers (no env)
        "pushq %11\n\t" // Arg 3
        "pushq %10\n\t" // Arg 2
        "pushq %9\n\t" // Arg 1
        "pushq %8\n\t" // Arg 0
        "pushq %7\n\t" // Argument count
        "movq $0, %%rdx\n\t" //fini should be null for now
        "jmpq  *%0\n\t"
        :
        : "r"(ep), 
          "r"(*((u64*)&auxv_r)), "r"(*(((u64*)&auxv_r)+1)), 
          "r"(*((u64*)&auxv_v)), "r"(*(((u64*)&auxv_v)+1)), 
          "r"(*((u64*)&auxv_p)), "r"(*(((u64*)&auxv_p)+1)), 
          "r"(_argc), "r"(argv[0]), "r"(argv[1]), "r"(argv[2]), "r"(argv[3]));
}

#endif /* ARCH_ELF_HH */
