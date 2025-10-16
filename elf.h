#ifndef _ELF
#define _ELF
#include <stdint.h>
#define ISELF(buf)((*(buf)==0x7F)&&(*(buf+1)=='E')&&(*(buf+2)=='L')&&(*(buf+3)=='F'))
#define EM_I386 3
#define EM_X86_64 0x3E
#define PT_LOAD 1
#define SHT_REL 9
#define SHT_RELA 4
#define SHT_STRTAB 3
enum elfType{
	ET_NONE = 0,	
	ET_REL = 1,
	ET_EXEC = 2,
	ET_DYN = 3,
};
struct elf32_ehdr{
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_hdr_size;
	uint16_t e_ph_size;
	uint16_t e_ph_cnt;
	uint16_t e_sh_size;
	uint16_t e_sh_cnt;
	uint16_t e_strtab_index;
};
struct elf32_shdr{
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entrysize;
};
struct elf32_phdr{
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_va;
	uint32_t p_pa;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
};
struct elf32_sym{
	uint32_t st_name;
	uint32_t st_value;
	uint32_t st_size;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t st_shndex;
};
struct elf32_rel{
	uint32_t r_offset;
	uint32_t r_info;
};
struct elf32_rela{
	uint32_t r_offset;
	uint32_t r_info;
	int32_t r_addend;
};
struct elf64_ehdr{
	uint8_t e_ident[16];
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
};
struct elf64_phdr{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_va;
	uint64_t p_pa;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};
struct elf64_shdr{
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
};
struct elf64_sym{
	uint32_t st_name;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t st_shndx;
	uint64_t st_value;
	uint64_t st_size;
};
struct elf64_rel{
	uint64_t r_offset;
	uint64_t r_info;
};
struct elf64_rela{
	uint64_t r_offset;
	uint64_t r_info;
	int64_t r_addend;
};
#endif
