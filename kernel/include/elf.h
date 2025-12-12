#ifndef _ELF
#define _ELF
#define ELF64_MIN_SIZE 64
#define ELF_SIGNATURE 0x464C457F
#define ELF_MACHINE_NONE 0x0
#define ELF_MACHINE_X64 0x3E
#define ELF_MACHINE_ARM64 0xB7
#define ELF_SHT_NULL 0x0
#define ELF_SHT_PROGRAM_DATA 0x01
#define ELF_SHT_SYMTAB 0x02
#define ELF_SHT_STRING_TABLE 0x03
#define ELF_SHT_RELA 0x04
#define ELF_SHT_HASHTAB 0x05
#define ELF_SHT_DYNINFO 0x06
#define ELF_SHT_NOTE 0x07
#define ELF_SHT_BSS 0x08
#define ELF_SHT_REL 0x09
#define ELF_SHT_RESERVED0 0x0A
#define ELF_SHT_DYN_LINK_SYMTAB 0x0B
#define ELF_PT_NULL 0x0
#define ELF_PT_LOAD 0x01
#define ELF_PT_DYN 0x02
#define ELF_PT_INTERPRETER 0x03
#define ELF_PT_NOTE 0x04
#define ELF_PT_RESERVED0 0x05
#define ELF_PT_PHDR 0x06
#define ELF_VALID_SIGNATURE(pIdent)(*(unsigned int*)pIdent==ELF_SIGNATURE)
struct elf64_header{
	unsigned char ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
}__attribute__((packed));
struct elf64_pheader{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_va;
	uint64_t p_pa;
	uint64_t p_fileSize;
	uint64_t p_memorySize;
	uint64_t p_align;
}__attribute__((packed));
struct elf64_sheader{
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addressAlign;
	uint64_t sh_entrySize;
}__attribute__((packed));
#endif
