#ifndef _FONT_DRIVER_TTF
#define _FONT_DRIVER_TTF
#include "subsystem/text.h"
#include "math/vector.h"
struct ttf_font_desc;
struct ttf_subtable_cmap_header;
struct ttf_bytecode_context_desc;
struct ttf_execution_code;
typedef int(*TTFGetGlyphIdFunc)(struct ttf_font_desc* pFontDesc, struct ttf_subtable_cmap_header* pCharacterMapSubtableHeader, uint32_t characterCode, uint32_t* pGlyphId);
typedef int(*TTFBytecodeInstructionFunc)(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_execution_code* pExecutionCode);
#define TTF_SIGNATURE ((uint32_t)0x00010000)

#define TTF_MAX_BYTECODE_CONTEXT_COUNT (8192)
#define TTF_DEFAULT_BYTECODE_STACK_SIZE (16384)

#define TTF_HEAD_TABLE_MAGIC_NUMBER ((uint32_t)0x5F0F3CF5)
#define TTF_CHECKSUM_MAGIC ((uint32_t)0xB1B0AFBA)

#define TTF_PLATFORM_ID_UNICODE (0x00)
#define TTF_PLATFORM_ID_MACINTOSH (0x01)
#define TTF_PLATFORM_ID_WINDOWS (0x03)

#define TTF_TABLE_TYPE_CMAP ((uint32_t)'pamc')
#define TTF_TABLE_TYPE_GLYF ((uint32_t)'fylg')
#define TTF_TABLE_TYPE_HEAD ((uint32_t)'daeh')
#define TTF_TABLE_TYPE_LOCA ((uint32_t)'acol')
#define TTF_TABLE_TYPE_MAXP ((uint32_t)'pxam')
#define TTF_TABLE_TYPE_NAME ((uint32_t)'eman')
#define TTF_TABLE_TYPE_POST ((uint32_t)'tsop')

#define TTF_TABLE_TYPE_HHEA ((uint32_t)'aehh')
#define TTF_TABLE_TYPE_HMTX ((uint32_t)'xtmh')
#define TTF_TABLE_TYPE_VHEA ((uint32_t)'aehv')
#define TTF_TABLE_TYPE_VMTX ((uint32_t)'xtmv')
#define TTF_TABLE_TYPE_OS2 ((uint32_t)'2/SO')
#define TTF_TABLE_TYPE_KERN ((uint32_t)'nrek')

#define TTF_TABLE_TYPE_CVT ((uint32_t)' tvc')
#define TTF_TABLE_TYPE_FPGM ((uint32_t)'mgpf')
#define TTF_TABLE_TYPE_PREP ((uint32_t)'perp')
#define TTF_TABLE_TYPE_GASP ((uint32_t)'psag')

#define TTF_TABLE_TYPE_GDEF ((uint32_t)'FEDG')
#define TTF_TABLE_TYPE_GSUB ((uint32_t)'BUSG')
#define TTF_TABLE_TYPE_GPOS ((uint32_t)'SOPG')
#define TTF_TABLE_TYPE_BASE ((uint32_t)'esab')

#define TTF_TABLE_ID_CMAP (0x00)
#define TTF_TABLE_ID_GLYF (0x01)
#define TTF_TABLE_ID_HEAD (0x02)
#define TTF_TABLE_ID_LOCA (0x03)
#define TTF_TABLE_ID_MAXP (0x04)
#define TTF_TABLE_ID_NAME (0x05)
#define TTF_TABLE_ID_POST (0x06)
#define TTF_TABLE_ID_HHEA (0x07)
#define TTF_TABLE_ID_HMTX (0x08)
#define TTF_TABLE_ID_VHEA (0x09)
#define TTF_TABLE_ID_VMTX (0x0A)
#define TTF_TABLE_ID_OS2 (0x0B)
#define TTF_TABLE_ID_KERN (0x0C)
#define TTF_TABLE_ID_CVT (0x0D)
#define TTF_TABLE_ID_FPGM (0x0E)
#define TTF_TABLE_ID_PREP (0x0F)
#define TTF_TABLE_ID_GASP (0x10)
#define TTF_TABLE_ID_GDEF (0x11)
#define TTF_TABLE_ID_GSUB (0x12)
#define TTF_TABLE_ID_GPOS (0x13)
#define TTF_TABLE_ID_BASE (0x14)

#define TTF_NAME_TYPE_COPYRIGHT (0x00)
#define TTF_NAME_TYPE_FAMILY_NAME (0x01)
#define TTF_NAME_TYPE_SUBFAMILY_NAME (0x02)
#define TTF_NAME_TYPE_UNIQUE_IDENT (0x03)
#define TTF_NAME_TYPE_FULL_NAME (0x04)
#define TTF_NAME_TYPE_VERSION_STRING (0x05)
#define TTF_NAME_TYPE_POSTSCRIPT_NAME (0x06)
#define TTF_NAME_TYPE_TRADEMARK (0x07)
#define TTF_NAME_TYPE_MANUFACTURER_NAME (0x08)
#define TTF_NAME_TYPE_DESIGNER (0x09)
#define TTF_NAME_TYPE_DESC (0x0A)
#define TTF_NAME_TYPE_URL_VENDOR (0x0B)
#define TTF_NAME_TYPE_URL_DESIGNER (0x0C)
#define TTF_NAME_TYPE_LICENSE_DESC (0x0D)
#define TTF_NAME_TYPE_LICENSE_INFO_URL (0x0E)
#define TTF_NAME_TYPE_RESERVED0 (0x0F)

#define TTF_OPCODE_NPUSHB (0x40)
#define TTF_OPCODE_NPUSHW (0x41)
#define TTF_OPCODE_PUSHB (0xB0)
#define TTF_OPCODE_PUSHW (0xB8)

#define TTF_OPCODE_AA (0x7F)
#define TTF_OPCODE_ABS (0x64)
#define TTF_OPCODE_ADD (0x60)
#define TTF_OPCODE_ALIGNPTS (0x27)
#define TTF_OPCODE_ALIGNRP (0x3C)
#define TTF_OPCODE_AND (0x5A)

#define TTF_OPCODE_CALL (0x2B)
#define TTF_OPCODE_CEILING (0x67)
#define TTF_OPCODE_CINDEX (0x25)
#define TTF_OPCODE_CLEAR (0x22)

#define TTF_OPCODE_DEBUG (0x4F)
#define TTF_OPCODE_DELTAC1 (0x73)
#define TTF_OPCODE_DELTAC2 (0x74)
#define TTF_OPCODE_DELTAC3 (0x75)
#define TTF_OPCODE_DELTAP1 (0x5D)
#define TTF_OPCODE_DELTAP2 (0x71)
#define TTF_OPCODE_DELTAP3 (0x72)
#define TTF_OPCODE_DEPTH (0x24)
#define TTF_OPCODE_DIV (0x62)
#define TTF_OPCODE_DUP (0x20)

#define TTF_OPCODE_EIF (0x59)
#define TTF_OPCODE_ELSE (0x1B)
#define TTF_OPCODE_ENDF (0x2D)
#define TTF_OPCODE_EQ (0x54)
#define TTF_OPCODE_EVEN (0x57)

#define TTF_OPCODE_FDEF (0x2C)
#define TTF_OPCODE_FLIP_OFF (0x4E)
#define TTF_OPCODE_FLIP_ON (0x4D)
#define TTF_OPCODE_FLIP_PT (0x80)
#define TTF_OPCODE_FLIP_PRGOFF (0x80)
#define TTF_OPCODE_FLIP_PRGON (0x81)
#define TTF_OPCODE_FLOOR (0x66)

#define TTF_OPCODE_GC (0x46)
#define TTF_OPCODE_GETINFO (0x88)
#define TTF_OPCODE_GFV (0x0D)
#define TTF_OPCODE_GPV (0x0C)
#define TTF_OPCODE_GT (0x52)
#define TTF_OPCODE_GTEQ (0x53)

#define TTF_OPCODE_IDEF (0x89)
#define TTF_OPCODE_IF (0x58)
#define TTF_OPCODE_INSTCTRL (0x8E)
#define TTF_OPCODE_IP (0x39)
#define TTF_OPCODE_ISECT (0x0F)
#define TTF_OPCODE_IUP (0x30)

#define TTF_OPCODE_JMPR (0x1C)
#define TTF_OPCODE_JROF (0x79)
#define TTF_OPCODE_JROT (0x78)

#define TTF_OPCODE_LOOPCALL (0x2A)
#define TTF_OPCODE_LT (0x50)
#define TTF_OPCODE_LTEQ (0x51)

#define TTF_OPCODE_MAX (0x8B)
#define TTF_OPCODE_MD (0x49)
#define TTF_OPCODE_MDAP (0x2E)
#define TTF_OPCODE_MDRP (0xC0)
#define TTF_OPCODE_MIAP (0x3E)
#define TTF_OPCODE_MIN (0x8C)
#define TTF_OPCODE_MINDEX (0x26)
#define TTF_OPCODE_MIRP (0xE0)
#define TTF_OPCODE_MPPEM (0x4B)
#define TTF_OPCODE_MPS (0x4C)
#define TTF_OPCODE_MSIRP (0x3A)
#define TTF_OPCODE_MUL (0x63)

#define TTF_OPCODE_NEG (0x65)
#define TTF_OPCODE_NEQ (0x55)
#define TTF_OPCODE_NOT (0x5C)
#define TTF_OPCODE_NROUND (0x6C)

#define TTF_OPCODE_ODD (0x56)
#define TTF_OPCODE_OR (0x5B)

#define TTF_OPCODE_POP (0x21)

#define TTF_OPCODE_RCVT (0x45)
#define TTF_OPCODE_RDTG (0x7D)
#define TTF_OPCODE_ROFF (0x7A)
#define TTF_OPCODE_ROLL (0x8A)
#define TTF_OPCODE_ROUND (0x68)
#define TTF_OPCODE_RS (0x43)
#define TTF_OPCODE_RTDG (0x3D)
#define TTF_OPCODE_RTG (0x18)
#define TTF_OPCODE_RTHG (0x19)
#define TTF_OPCODE_RUTG (0x7C)

#define TTF_OPCODE_S45ROUND (0x77)
#define TTF_OPCODE_SANGW (0x7E)
#define TTF_OPCODE_SCANCTRL (0x85)
#define TTF_OPCODE_SCANTYPE (0x8D)
#define TTF_OPCODE_SCFS (0x48)
#define TTF_OPCODE_SCVTCI (0x1D)
#define TTF_OPCODE_SDB (0x5E)
#define TTF_OPCODE_SDPVTL (0x86)
#define TTF_OPCODE_SDS (0x5F)
#define TTF_OPCODE_SFVFS (0x0B)
#define TTF_OPCODE_SFVTCA (0x04)
#define TTF_OPCODE_SFVTL (0x08)
#define TTF_OPCODE_SFVTPV (0x0E)
#define TTF_OPCODE_SHC (0x34)
#define TTF_OPCODE_SHP (0x32)
#define TTF_OPCODE_SHPIX (0x38)
#define TTF_OPCODE_SHZ (0x36)
#define TTF_OPCODE_SLOOP (0x17)
#define TTF_OPCODE_SMD (0x1A)
#define TTF_OPCODE_SPVFS (0x0A)
#define TTF_OPCODE_SPVTCA (0x02)
#define TTF_OPCODE_SPVTL (0x06)
#define TTF_OPCODE_SROUND (0x76)
#define TTF_OPCODE_SRP0 (0x10)
#define TTF_OPCODE_SRP1 (0x11)
#define TTF_OPCODE_SRP2 (0x12)
#define TTF_OPCODE_SSW (0x1F)
#define TTF_OPCODE_SSWCI (0x1E)
#define TTF_OPCODE_SUB (0x61)
#define TTF_OPCODE_SVTCA (0x00)
#define TTF_OPCODE_SWAP (0x23)
#define TTF_OPCODE_SZP0 (0x13)
#define TTF_OPCODE_SZP1 (0x14)
#define TTF_OPCODE_SZP2 (0x15)
#define TTF_OPCODE_SZPS (0x16)

#define TTF_OPCODE_UTP (0x29)

#define TTF_OPCODE_WCVTF (0x70)
#define TTF_OPCODE_WCVTP (0x44)
#define TTF_OPCODE_WS (0x42)

#define TTF_EXECUTION_CODE_UNKNOWN (0x00)
#define TTF_EXECUTION_CODE_SUCCESS (0x01)
#define TTF_EXECUTION_CODE_OPERATION_FAILURE (0x02)
#define TTF_EXECUTION_CODE_OPERATION_UNSUPPORTED (0x03)

struct ttf_header{
	uint32_t signature;
	uint16_t table_count;
	uint16_t search_range;
	uint16_t entry_selector;
	uint16_t range_shift;
}__attribute__((packed));
struct ttf_table_desc{
	uint32_t table_type;
	uint32_t table_checksum;
	uint32_t table_offset;
	uint32_t table_length;
}__attribute__((packed));
struct ttf_table_cmap_record{
	uint16_t platform_id;
	uint16_t encoding_id;
	uint32_t subtable_offset;
}__attribute__((packed));
struct ttf_table_cmap_header{
	uint16_t version;
	uint16_t record_count;
	struct ttf_table_cmap_record record_list[];
}__attribute__((packed));
struct ttf_subtable_cmap_header{
	uint16_t format;
}__attribute__((packed));
struct ttf_subtable_cmap4{
	struct ttf_subtable_cmap_header header;
	uint16_t subtable_length;
	uint16_t language_id;
	uint16_t segment_count_x2;
	uint16_t search_range;
	uint16_t entry_selector;
	uint16_t range_shift;
	uint8_t lookup_data[];
}__attribute__((packed));
struct ttf_subtable_cmap12_group{
	uint32_t start_char_code;
	uint32_t end_char_code;
	uint32_t start_glyph_id;
}__attribute__((packed));
struct ttf_subtable_cmap12{
	struct ttf_subtable_cmap_header header;
	uint16_t reserved0;
	uint32_t subtable_length;
	uint32_t language_id;
	uint32_t group_count;
	struct ttf_subtable_cmap12_group group_list[];
}__attribute__((packed));
struct ttf_glyf_header{
	int16_t contour_count;
	int16_t min_x;
	int16_t min_y;
	int16_t max_x;
	int16_t max_y;
}__attribute__((packed));
struct ttf_glyf_point_flags{
	uint8_t on_curve:1;
	uint8_t x_short:1;
	uint8_t y_short:1;
	uint8_t repeat:1;
	uint8_t x_same:1;
	uint8_t y_same:1;
	uint8_t overlap:1;
	uint8_t reserved0:1;
}__attribute__((packed));
struct ttf_glyf_simple_desc{
	struct ttf_glyf_header header;
	uint8_t data[];
}__attribute__((packed));
struct ttf_glyf_composite_desc{
	struct ttf_glyf_header header;

}__attribute__((packed));
struct ttf_instruction{
	uint8_t opcode;
	uint8_t data[];
}__attribute__((packed));
struct ttf_table_head{
	uint16_t major_version;
	uint16_t minor_version;
	uint32_t font_revision;
	uint32_t checksum;
	uint32_t magic;
	uint16_t flags;
	uint16_t units_per_em;
	int64_t time_created;
	int64_t modified;
	int16_t min_x;
	int16_t min_y;
	int16_t max_x;
	int16_t max_y;
	uint16_t mac_style;
	uint16_t lowest_rec_ppem;
	int16_t font_direction_hint;
	int16_t index_to_loca_format;
	int16_t glyph_data_format;
}__attribute__((packed));
struct ttf_table_name_header{
	uint16_t format;
	uint16_t string_record_count;
	uint16_t string_data_offset;
}__attribute__((packed));
struct ttf_table_name_record{
	uint16_t platform_id;
	uint16_t encoding_id;
	uint16_t language_id;
	uint16_t name_id;
	uint16_t string_length;
	uint16_t string_offset;
}__attribute__((packed));
struct ttf_contour_entry_desc{
	uint64_t reserved0;
}__attribute__((packed));
struct ttf_point_entry_desc{
	struct uvec2_32 position;
	struct ttf_glyf_point_flags flags;
	uint8_t reserved0[7];
}__attribute__((packed));
struct ttf_pixel_entry_desc{
	uint16_t contourIndex;
	int16_t windingDelta;
	uint32_t nextActivePixelOffset;
}__attribute__((packed));
struct ttf_execution_code{
	uint16_t mainCode;
	uint16_t subCode;
};
struct ttf_bytecode_context_desc{
	struct ttf_font_desc* pFontDesc;
	uint64_t bytecodeContextId;
	uint8_t* pStack;
	uint8_t* pStackEntry;
	uint64_t stackSize;
	uint8_t* pBytecode;
	uint64_t bytecodeLength;
};
struct ttf_bytecode_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	uint64_t maxBytecodeContextCount;
};
struct ttf_font_desc{
	struct text_subsystem_font_desc* pSubsystemFontDesc;
	uint64_t bytecodeContextId;
	unsigned char* pFontBuffer; 
	uint64_t fontBufferSize;
	uint32_t* pGlyphIdMappingTable;
	uint64_t glyphIdMappingTableSize;
	uint64_t tableCount;
	struct ttf_table_desc* tableDescList[32];
};
struct font_driver_ttf_info{
	struct text_subsystem_font_driver_info subsystemFontDriverInfo;
	struct ttf_bytecode_subsystem_info bytecodeSubsystemInfo;
};
int font_driver_ttf_init(void);
int font_driver_ttf_deinit(void);
int ttf_table_get_desc(struct ttf_font_desc* pFontDesc, uint64_t tableId, struct ttf_table_desc** ppTableDesc);
int ttf_control_get_value(struct ttf_font_desc* pFontDesc, uint32_t controlValueIndex, double* pControlValue);
int ttf_opcode_get_name(uint8_t opcode, const unsigned char** ppOpcodeName);
int ttf_execution_code_get_name(uint64_t executionCode, const unsigned char** ppExecutionCodeName);
int ttf_bytecode_context_init(struct ttf_font_desc* pFontDesc, uint64_t stackSize, uint64_t* pBytecodeContextId);
int ttf_bytecode_context_deinit(uint64_t bytecodeContextId);
int ttf_bytecode_context_get_desc(uint64_t bytecodeContextId, struct ttf_bytecode_context_desc** ppBytecodeContextDesc);
int ttf_bytecode_context_set_bytecode(uint64_t bytecodeCntextId, uint8_t* pBytecode, uint64_t bytecodeLength);
int ttf_bytecode_context_stack_clear(uint64_t bytecodeContextId);
int ttf_bytecode_context_push(uint64_t bytecodeContextId, uint8_t* pStackData, uint64_t stackDataSize);
int ttf_bytecode_context_pop(uint64_t bytecodeContextId, uint8_t** ppStackData, uint64_t stackDataSize);
int ttf_bytecode_context_execute(uint64_t bytecodeContextId, struct ttf_execution_code* pExecutionCode);
int ttf_glyph_get_id_fmt_4(struct ttf_font_desc* pFontDesc, struct ttf_subtable_cmap_header* pCharacterMapSubtableHeader, uint32_t characterCode, uint32_t* pGlyphId);
int ttf_glyph_get_id(struct ttf_font_desc* pFontDesc, uint16_t platformId, uint16_t encodingId, uint32_t characterCode, uint32_t* pGlyphId);
int ttf_glyph_get_glyf_table(struct ttf_font_desc* pFontDesc, uint32_t glyphId, unsigned char** ppGlyfTableBase, uint32_t* pGlyfTableLength);
int ttf_glyf_point_get_vector(struct ttf_font_desc* pFontDesc, uint32_t glyphId, uint64_t pointFlagsIndex, struct uvec2_32 vectorIndexList, struct uvec2_32* pVector, struct uvec2_32* pNewVectorIndexList);
int ttf_glyf_tesselate(struct ttf_font_desc* pFontDesc, uint32_t glyphId, int16_t* pTextureBuffer, struct uvec2_32 textureBufferRect);
int ttf_font_load(unsigned char* pFontBuffer, uint64_t fontBufferSize, struct text_subsystem_font_desc* pSubsystemFontDesc);
int ttf_font_unload(struct text_subsystem_font_desc* pSubsystemFontDesc);
int ttf_font_verify(unsigned char* pFontBuffer, uint64_t fontBufferSize);
int ttf_font_glyph_get_id(struct text_subsystem_font_desc* pSubsystemFontDesc, uint64_t characterCode, uint64_t* pGlyphId);
int ttf_font_glyph_tesselate(struct text_subsystem_font_desc* pSubsystemFontDesc, uint32_t glyphId, int16_t* pTextureBuffer, struct uvec2_32 textureBufferRect);
#endif
