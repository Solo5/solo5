#ifndef __UKVM_CPU_H__
#define __UKVM_CPU_H__

#ifndef _BITUL

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#define _BITUL(x)	(_AC(1,UL) << (x))
#define _BITULL(x)	(_AC(1,ULL) << (x))

#endif

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */
#define X86_EFLAGS_TF_BIT   8 /* Trap flag (single step) */
#define X86_EFLAGS_TF   _BITUL(X86_EFLAGS_TF_BIT)

/*
 * Basic CPU control in CR0
 */
#define	X86_CR0_PE	0x00000001	/* Protected mode Enable */
#define	X86_CR0_NE	0x00000020	/* Numeric Error enable (EX16 vs IRQ13) */
#define	X86_CR0_PG	0x80000000	/* PaGing enable */
#define	X86_CR0_NW  0x20000000	/* Not Write-through */
#define	X86_CR0_CD  0x40000000	/* Cache Disable */
#define	X86_CR0_MP	0x00000002	/* "Math" (fpu) Present */
#define	X86_CR0_EM	0x00000004	/* EMulate FPU instructions. (trap ESC only) */

/*
 * Intel CPU features in CR4
 */
#define	X86_CR4_PAE	 0x00000020	/* Physical address extension */
#define	X86_CR4_VMXE 0x00002000	/* enable VMX operation (Intel-specific) */
#define	X86_CR4_FXSR 0x00000200	/* Fast FPU save/restore used by OS */
#define	X86_CR4_XMM	 0x00000400	/* enable SIMD/MMX2 to use except 16 */


#define	X86_EFER_LME 0x000000100	/* Long mode enable (R/W) */
#define	X86_EFER_LMA 0x000000400	/* Long mode active (R) */

/* AMD64 MSR's */
#define	MSR_EFER	0xc0000080	/* extended features */
#define	MSR_STAR	0xc0000081	/* legacy mode SYSCALL target/cs/ss */
#define	MSR_LSTAR	0xc0000082	/* long mode SYSCALL target rip */
#define	MSR_CSTAR	0xc0000083	/* compat mode SYSCALL target rip */
#define	MSR_SF_MASK	0xc0000084	/* syscall flags mask */
#define	MSR_FSBASE	0xc0000100	/* base address of the %fs "segment" */
#define	MSR_GSBASE	0xc0000101	/* base address of the %gs "segment" */
#define	MSR_KGSBASE	0xc0000102	/* base address of the kernel %gs */
#define	MSR_SYSENTER_CS_MSR	0x174
#define	MSR_SYSENTER_ESP_MSR	0x175
#define	MSR_SYSENTER_EIP_MSR	0x176

/*
 * Intel long mode page directory/table entries
 */
#define X86_PDPT_P_BIT          0 /* Present */
#define X86_PDPT_P              _BITUL(X86_PDPT_P_BIT)
#define X86_PDPT_RW_BIT         1 /* Writable */
#define X86_PDPT_RW             _BITUL(X86_PDPT_RW_BIT)
#define X86_PDPT_PS_BIT         7 /* Page size */
#define X86_PDPT_PS             _BITUL(X86_PDPT_PS_BIT)

/*
 * GDT and KVM segment manipulation
 */

#define GDT_DESC_OFFSET(n) ((n) * 0x8)

#define GDT_GET_BASE(x) (                      \
    (((x) & 0xFF00000000000000) >> 32) |       \
    (((x) & 0x000000FF00000000) >> 16) |       \
    (((x) & 0x00000000FFFF0000) >> 16))

#define GDT_GET_LIMIT(x) (__u32)(                                      \
                                 (((x) & 0x000F000000000000) >> 32) |  \
                                 (((x) & 0x000000000000FFFF)))

/* Constructor for a conventional segment GDT (or LDT) entry */
/* This is a macro so it can be used in initializers */
#define GDT_ENTRY(flags, base, limit)               \
    ((((base)  & _AC(0xff000000, ULL)) << (56-24)) | \
     (((flags) & _AC(0x0000f0ff, ULL)) << 40) |      \
     (((limit) & _AC(0x000f0000, ULL)) << (48-16)) | \
     (((base)  & _AC(0x00ffffff, ULL)) << 16) |      \
     (((limit) & _AC(0x0000ffff, ULL))))

struct _kvm_segment {
    uint64_t base;
    uint32_t limit;
    uint16_t selector;
    uint8_t type;
    uint8_t present, dpl, db, s, l, g, avl;
    uint8_t unusable;
    uint8_t padding;
};

#define GDT_GET_G(x)   (uint8_t)(((x) & 0x0080000000000000) >> 55)
#define GDT_GET_DB(x)  (uint8_t)(((x) & 0x0040000000000000) >> 54)
#define GDT_GET_L(x)   (uint8_t)(((x) & 0x0020000000000000) >> 53)
#define GDT_GET_AVL(x) (uint8_t)(((x) & 0x0010000000000000) >> 52)
#define GDT_GET_P(x)   (uint8_t)(((x) & 0x0000800000000000) >> 47)
#define GDT_GET_DPL(x) (uint8_t)(((x) & 0x0000600000000000) >> 45)
#define GDT_GET_S(x)   (uint8_t)(((x) & 0x0000100000000000) >> 44)
#define GDT_GET_TYPE(x)(uint8_t)(((x) & 0x00000F0000000000) >> 40)

#define GDT_TO_KVM_SEGMENT(seg, gdt_table, sel) \
    do {                                        \
        uint64_t gdt_ent = gdt_table[sel];         \
        seg.base = GDT_GET_BASE(gdt_ent);       \
        seg.limit = GDT_GET_LIMIT(gdt_ent);     \
        seg.selector = sel * 8;                 \
        seg.type = GDT_GET_TYPE(gdt_ent);       \
        seg.present = GDT_GET_P(gdt_ent);       \
        seg.dpl = GDT_GET_DPL(gdt_ent);         \
        seg.db = GDT_GET_DB(gdt_ent);           \
        seg.s = GDT_GET_S(gdt_ent);             \
        seg.l = GDT_GET_L(gdt_ent);             \
        seg.g = GDT_GET_G(gdt_ent);             \
        seg.avl = GDT_GET_AVL(gdt_ent);         \
    } while (0)

#endif

