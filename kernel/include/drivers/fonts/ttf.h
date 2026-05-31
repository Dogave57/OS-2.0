#ifndef _FONT_DRIVER_TTF
#define _FONT_DRIVER_TTF
#include "subsystem/text.h"
#include "math/vector.h"
struct ttf_font_desc;
struct ttf_subtable_cmap_header;
typedef int(*TTFGetGlyphIdFunc)(struct ttf_font_desc* pFontDesc, struct ttf_subtable_cmap_header* pCharacterMapSubtableHeader, uint32_t characterCode, uint32_t* pGlyphId);
#define TTF_SIGNATURE ((uint32_t)0x00010000)

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
struct ttf_point_entry_desc{
	struct uvec2_32 position;
}__attribute__((packed));
struct ttf_pixel_entry_desc{
	uint16_t pointIndex;
	uint16_t pointCount;
}__attribute__((packed));
struct ttf_font_desc{
	struct text_subsystem_font_desc* pSubsystemFontDesc;
	unsigned char* pFontBuffer; 
	uint64_t fontBufferSize;
	uint32_t* pGlyphIdMappingTable;
	uint64_t glyphIdMappingTableSize;
	uint64_t tableCount;
	struct ttf_table_desc* tableDescList[32];
};
struct font_driver_ttf_info{
	uint64_t fontDriverId;
	struct text_subsystem_font_driver_info subsystemFontDriverInfo;
};
int font_driver_ttf_init(void);
int ttf_table_get_desc(struct ttf_font_desc* pFontDesc, uint64_t tableId, struct ttf_table_desc** ppTableDesc);
int ttf_glyph_get_id_fmt_4(struct ttf_font_desc* pFontDesc, struct ttf_subtable_cmap_header* pCharacterMapSubtableHeader, uint32_t characterCode, uint32_t* pGlyphId);
int ttf_glyph_get_id(struct ttf_font_desc* pFontDesc, uint16_t platformId, uint16_t encodingId, uint32_t characterCode, uint32_t* pGlyphId);
int ttf_glyph_get_glyf_table(struct ttf_font_desc* pFontDesc, uint32_t glyphId, unsigned char** ppGlyfTableBase, uint32_t* pGlyfTableLength);
int ttf_glyf_point_get_vector(struct ttf_font_desc* pFontDesc, uint32_t glyphId, uint64_t pointFlagsIndex, struct uvec2_32 vectorIndexList, struct uvec2_32* pVector, struct uvec2_32* pNewVectorIndexList);
int ttf_glyf_tesselate(struct ttf_font_desc* pFontDesc, uint32_t glyphId, float* pTextureBufefr, struct uvec2_32 textureBufferRect);
int ttf_font_load(unsigned char* pFontBuffer, uint64_t fontBufferSize, struct text_subsystem_font_desc* pSubsystemFontDesc);
int ttf_font_unload(struct text_subsystem_font_desc* pSubsystemFontDesc);
int ttf_font_verify(unsigned char* pFontBuffer, uint64_t fontBufferSize);
int ttf_font_glyph_get_id(struct text_subsystem_font_desc* pSubsystemFontDesc, uint64_t characterCode, uint64_t* pGlyphId);
int ttf_font_glyph_tesselate(struct text_subsystem_font_desc* pSubsystemFontDesc, uint32_t glyphId, float* pTextureBuffer, struct uvec2_32 textureBufferRect);
#endif
