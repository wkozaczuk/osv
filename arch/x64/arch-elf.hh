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

inline void elf_entry_point(void* ep, int argc, char** argv, u64 *random_bytes)
{
    //u64 _argc = argc;
    struct auxv_t auxv;
    auxv.a_type = _AT_RANDOM;
    auxv.a_un.a_val = reinterpret_cast<uint64_t>(random_bytes);

    u64 _argc = argc;
    //char *arg0 = argv[0];
    //char *arg1 = argv[1];

    // Needs to be 16 bytes aligned
    asm volatile (
        //"pushq $0\n\t" // Padding
        "pushq $0\n\t" // Zero AUX
        "pushq $0\n\t" // Zero AUX
        "pushq %2\n\t" // AT_RANDOM AUX
        "pushq %1\n\t" // AT_RANDOM AUX
        "pushq $0\n\t" // Zero
        "pushq %0\n\t" // End of environment pointers (no env)
        "pushq %5\n\t" // Arg 1
        "pushq %4\n\t" // Arg 0
        "pushq %3\n\t" // Argument count
        "movq $0, %%rdx\n\t" //fini should be null for now
        "jmpq  *%0\n\t"
        :
        //: "r"(ep), "r"(*((u64*)&auxv)), "r"(*(((u64*)&auxv)+1)), "r"(_argc));
        : "r"(ep), "r"(*((u64*)&auxv)), "r"(*(((u64*)&auxv)+1)), "r"(_argc), "r"(argv[0]), "r"(argv[1]));
        //: "r"(ep), "r"(_argc), "r"(argv));
}

#endif /* ARCH_ELF_HH */
