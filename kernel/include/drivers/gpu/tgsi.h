#ifndef _TGSI
#define _TGSI
#include "mem/vmm.h"
#include "subsystem/subsystem.h"
#define TGSI_MAX_CONTEXT_COUNT (8192)
#define TGSI_DEFAULT_BYTECODE_COMMIT (PAGE_SIZE*4)
#define TGSI_DEFAULT_BYTECODE_RESERVE (MEM_MB*sizeof(uint32_t))

#define TGSI_PROCESSOR_TYPE_VERTEX (0x00)
#define TGSI_PROCESSOR_TYPE_FRAGMENT (0x01)
#define TGSI_PROCESSOR_TYPE_GEOMETRY (0x02)
#define TGSI_PROCESSOR_TYPE_TESS_CONTROL (0x03)
#define TGSI_PROCESSOR_TYPE_TESS_EVALUATE (0x04)
#define TGSI_PROCESSOR_TYPE_COMPUTE (0x05)

#define TGSI_TOKEN_TYPE_DECLARATION (0x00)
#define TGSI_TOKEN_TYPE_IMMEDIATE (0x01)
#define TGSI_TOKEN_TYPE_INSTRUCTION (0x02)
#define TGSI_TOKEN_TYPE_PROPERTY (0x03)

#define TGSI_DATA_TYPE_FLOAT32 (0x00)
#define TGSI_DATA_TYPE_UINT32 (0x01)
#define TGSI_DATA_TYPE_INT32 (0x02)
#define TGSI_DATA_TYPE_FLOAT64 (0x03)
#define TGSI_DATA_TYPE_UINT64 (0x04)
#define TGSI_DATA_TYPE_INT64 (0x05)
#define TGSI_OPCODE_ARL (0)
#define TGSI_OPCODE_MOV (1)
#define TGSI_OPCODE_RCP (3)
#define TGSI_OPCODE_LOG (6)
#define TGSI_OPCODE_MUL (7)
#define TGSI_OPCODE_ADD (8)
#define TGSI_OPCODE_DST (11)
#define TGSI_OPCODE_MIN (12)
#define TGSI_OPCODE_MAX (13)
#define TGSI_OPCODE_LRP (18)
#define TGSI_OPCODE_SQRT (20)
#define TGSI_OPCODE_ROUND (27)
#define TGSI_OPCODE_POW (30)
#define TGSI_OPCODE_COS (36)
#define TGSI_OPCODE_KILL (39)
#define TGSI_OPCODE_SIN (48)
#define TGSI_OPCODE_END (117)

#define TGSI_FILE_TYPE_NULL (0x00)
#define TGSI_FILE_TYPE_CONSTANT (0x01)
#define TGSI_FILE_TYPE_INPUT (0x02)
#define TGSI_FILE_TYPE_OUTPUT (0x03)
#define TGSI_FILE_TYPE_TEMPORARY (0x04)
#define TGSI_FILE_TYPE_SAMPLER (0x05)
#define TGSI_FILE_TYPE_ADDRESS (0x06)
#define TGSI_FILE_TYPE_IMMEDIATE (0x07)
#define TGSI_FILE_TYPE_SYSTEM_VALUE (0x08)
#define TGSI_FILE_TYPE_IMAGE (0x09)
#define TGSI_FILE_TYPE_SAMPLER_VIEW (0x0A)
#define TGSI_FILE_TYPE_BUFFER (0x0B)
#define TGSI_FILE_TYPE_MEMORY (0x0C)
#define TGSI_FILE_TYPE_CONSTANT_BUFFER (0x0D)
#define TGSI_FILE_TYPE_HW_ATOMIC (0x0E)

#define TGSI_INTERPOLATE_CONSTANT (0x00)
#define TGSI_INTERPOLATE_LINEAR (0x01)
#define TGSI_INTERPOLATE_PERSPECTIVE (0x02)
#define TGSI_INTERPOLATE_COLOR (0x03)

#define TGSI_SEMANTIC_POSITION (0x00)
#define TGSI_SEMANTIC_COLOR (0x01)
#define TGSI_SEMANTIC_BCOLOR (0x02)
#define TGSI_SEMANTIC_FOG (0x03)
#define TGSI_SEMANTIC_PSIZE (0x04)
#define TGSI_SEMANTIC_GENERIC (0x05)
#define TGSI_SEMANTIC_NORMAL (0x06)
#define TGSI_SEMANTIC_FACE (0x07)
#define TGSI_SEMANTIC_EDGEFLAG (0x08)
#define TGSI_SEMANTIC_PRIMID (0x09)
#define TGSI_SEMANTIC_INSTANCE_ID (0x0A)
#define TGSI_SEMANTIC_VERTEX_ID (0x0B)
#define TGSI_SEMANTIC_STENCIL (0x0C)
#define TGSI_SEMANTIC_CLIP_DIST (0x0D)
#define TGSI_SEMANTIC_CLIP_VERTEX (0x0E)
#define TGSI_SEMANTIC_GRID_SIZE (0x0F)
#define TGSI_SEMANTIC_BLOCK_ID (0x10)
#define TGSI_SEMANTIC_BLOCK_SIZE (0x11)
#define TGSI_SEMANTIC_THREAD_ID (0x12)
#define TGSI_SEMANTIC_TEXCOORD (0x13)
#define TGSI_SEMANTIC_PCOORD (0x14)
#define TGSI_SEMANTIC_VIEWPORT_INDEX (0x15)
#define TGSI_SEMANTIC_LAYER (0x16)
#define TGSI_SEMANTIC_SAMPLE_ID (0x17)
#define TGSI_SEMANTIC_SAMPLE_POS (0x18)
#define TGSI_SEMANTIC_SAMPLE_MASK (0x19)
#define TGSI_SEMANTIC_INVOCATION_ID (0x1A)
#define TGSI_SEMANTIC_VERTEX_ID_NO_BASE (0x1B)
#define TGSI_SEMANTIC_BASE_VERTEX (0x1C)
#define TGSI_SEMANTIC_PATCH (0x1D)
#define TGSI_SEMANTIC_TESSCOORD (0x1E)
#define TGSI_SEMANTIC_TESSOUTER (0x1F)
#define TGSI_SEMANTIC_TESSINNER (0x20)
#define TGSI_SEMANTIC_VERTICES_IN_COUNT (0x21)
#define TGSI_SEMANTIC_HELPER_INVOCATION (0x22)
#define TGSI_SEMANTIC_BASE_INSTANCE (0x23)
#define TGSI_SEMANTIC_DRAW_ID (0x24)
#define TGSI_SEMANTIC_WORK_DIM (0x25)
#define TGSI_SEMANTIC_SUBGROUP_SIZE (0x26)
#define TGSI_SEMANTIC_SUBGROUP_INVOCATION (0x27)
#define TGSI_SEMANTIC_SUBGROUP_EQ_MASK (0x28)
#define TGSI_SEMANTIC_SUBGROUP_GE_MASK (0x29)
#define TGSI_SEMANTIC_SUBGROUP_GT_MASK (0x2A)
#define TGSI_SEMANTIC_SUBGROUP_LE_MASK (0x2B)
#define TGSI_SEMANTIC_SUBGROUP_LT_MASK (0x2C)
#define TGSI_SEMANTIC_CS_USER_DATA_AMD (0x2D)
#define TGSI_SEMANTIC_VIEWPORT_MASK (0x2E)

#define TGSI_WRITEMASK_NONE (0x00)
#define TGSI_WRITEMASK_X (0x01)
#define TGSI_WRITEMASK_Y (0x02)
#define TGSI_WRITEMASK_XY (0x03)
#define TGSI_WRITEMASK_Z (0x04)
#define TGSI_WRITEMASK_XZ (0x05)
#define TGSI_WRITEMASK_YZ (0x06)
#define TGSI_WRITEMASK_XYZ (0x07)
#define TGSI_WRITEMASK_W (0x08)
#define TGSI_WRITEMASK_XW (0x09)
#define TGSI_WRITEMASK_YW (0x0A)
#define TGSI_WRITEMASK_XYW (0x0B)
#define TGSI_WRITEMASK_ZW (0x0C)
#define TGSI_WRITEMASK_XZW (0x0D)
#define TGSI_WRITEMASK_YZW (0x0E)
#define TGSI_WRITEMASK_XYZW (0x0F)

struct tgsi_header{
	uint32_t header_size:8;
	uint32_t body_size:24;
}__attribute__((packed));
struct tgsi_processor{
	uint32_t processor:4;
	uint32_t padding0:24;
}__attribute__((packed));
struct tgsi_token{
	uint32_t token_type:4;
	uint32_t token_count:8;

	uint32_t extra:20;
}__attribute__((packed));
struct tgsi_declaration{
	uint32_t token_type:4;
	uint32_t token_count:8;

	uint32_t file_type:4;
	uint32_t writemask:4;
	uint32_t dimension:1;
	uint32_t semantic:1;
	uint32_t interpolate:1;
	uint32_t invariant:1;
	uint32_t local:1;
	uint32_t array:1;
	uint32_t atomic:1;
	uint32_t memory_type:2;
	uint32_t padding0:3;
}__attribute__((packed));
struct tgsi_declaration_range{
	uint32_t first:16;
	uint32_t last:16;
}__attribute__((packed));
struct tgsi_declaration_dimension{
	uint32_t index2D:16;
	uint32_t padding0:16;
}__attribute__((packed));
struct tgsi_declaration_interpolate{
	uint32_t interpolate:4;
	uint32_t location:2;
	uint32_t padding0:26;
}__attribute__((packed));
struct tgsi_declaration_semantic{
	uint32_t name:8;
	uint32_t index:16;
	uint32_t streamX:2;
	uint32_t streamY:2;
	uint32_t streamZ:2;
	uint32_t streamW:2;
}__attribute__((packed));
struct tgsi_immediate_data{
	float f;
	uint32_t ui;
	int32_t i;
}__attribute__((packed));
struct tgsi_immediate{
	uint32_t token_type:4;
	uint32_t token_count:14;

	uint32_t data_type:4;
	uint32_t padding0:10;
}__attribute__((packed));
struct tgsi_instruction{
	uint32_t token_type:4;
	uint32_t token_count:8;
	uint32_t opcode:8;
	uint32_t saturate:1;
	uint32_t destination_register_count:2;
	uint32_t source_register_count:4;
	uint32_t label:1;
	uint32_t texture:1;
	uint32_t memory:1;
	uint32_t precise:1;
	uint32_t padding0:1;
}__attribute__((packed));
struct tgsi_src_register{
	uint32_t file_type:4;
	uint32_t indirect:1;
	uint32_t dimension:1;
	int32_t index:16;
	uint32_t swizzleX:2;
	uint32_t swizzleY:2;
	uint32_t swizzleZ:2;
	uint32_t swizzleW:2;
	uint32_t absolute:1;
	uint32_t negate:1;
}__attribute__((packed));
struct tgsi_dest_register{
	uint32_t file_type:4;
	uint32_t writemask:4;
	uint32_t indirect:1;
	uint32_t dimension:1;
	int32_t index:16;
	uint32_t padding0:6;
}__attribute__((packed));
struct tgsi_property{
	uint32_t token_type:4;
	uint32_t token_count:8;

	uint32_t property_name:8;
	uint32_t padding0:12;
}__attribute__((packed));
struct tgsi_property_data{
	uint32_t data;
}__attribute__((packed));
struct tgsi_context_desc{
	uint64_t contextId;
	uint32_t* pBytecode;
	uint64_t bytecodeReserve;
	uint64_t bytecodeCommit;
	uint64_t tokenCount;
};
struct tgsi_driver_info{
	struct subsystem_desc* pContextSubsystemDesc;
};
int tgsi_driver_init(void);
int tgsi_context_init(struct tgsi_context_desc** ppContextDesc);
int tgsi_context_deinit(struct tgsi_context_desc* pContextDesc);
int tgsi_context_reset(struct tgsi_context_desc* pContextDesc);
int tgsi_context_get_bytecode_list(struct tgsi_context_desc* pContextDesc, uint32_t** ppBytecodeList);
int tgsi_context_get_bytecode_size(struct tgsi_context_desc* pContextDesc, uint64_t* pBytecodeSize);
int tgsi_emit_token(struct tgsi_context_desc* pContextDesc, uint32_t token);
int tgsi_emit_token_list(struct tgsi_context_desc* pContextDesc, uint64_t tokenCount, uint32_t* pTokenList);
int tgsi_compile_shader(struct tgsi_context_desc* pContextDesc, unsigned char* pShaderCode, uint64_t shaderCodeSize);
#endif
