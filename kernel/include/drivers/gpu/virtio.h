#ifndef _VIRTIO_GPU
#define _VIRTIO_GPU
#include "cpu/mutex.h"
#include "subsystem/gpu.h"
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#define VIRTIO_GPU_DEVICE_ID 0x1050
#define VIRTIO_GPU_BLOCK_BASE_REGISTERS (0x01)
#define VIRTIO_GPU_BLOCK_NOTIFY_REGISTERS (0x02)
#define VIRTIO_GPU_BLOCK_INTERRUPT_REGISTERS (0x03)
#define VIRTIO_GPU_BLOCK_DEVICE_REGISTERS (0x04)
#define VIRTIO_GPU_BLOCK_PCIE_REGISTERS (0x05)
#define VIRTIO_GPU_BLOCK_SHARED_MEMORY_REGISTERS (0x08)

#define VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY (0x00)
#define VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN (0x01)

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO (0x0100)
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D (0x0101)
#define VIRTIO_GPU_CMD_RESOURCE_FREE (0x0102)
#define VIRTIO_GPU_CMD_SET_SCANOUT (0x0103)
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH (0x0104)
#define VIRTIO_GPU_CMD_TRANSFER_H2D (0x0105)
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING (0x0106)
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING (0x0107)
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO (0x0108)
#define VIRTIO_GPU_CMD_GET_CAPSET (0x0109)
#define VIRTIO_GPU_CMD_GET_MONITOR_IDENT_INFO (0x010A)
#define VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID (0x010B)
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB (0x010C)
#define VIRTIO_GPU_CMD_SET_SCANOUT_BLOB (0x010D)

#define VIRTIO_GPU_CMD3D_CONTEXT_CREATE (0x0200)
#define VIRTIO_GPU_CMD3D_CONTEXT_DESTROY (0x0201)
#define VIRTIO_GPU_CMD3D_CONTEXT_ATTACH_RESOURCE (0x0202)
#define VIRTIO_GPU_CMD3D_CONTEXT_DETACH_RESOURCE (0x0203)
#define VIRTIO_GPU_CMD3D_RESOURCE_CREATE_3D (0x0204)
#define VIRTIO_GPU_CMD3D_TRANSFER_D2H (0x0205)
#define VIRTIO_GPU_CMD3D_TRANSFER_H2D (0x0206)
#define VIRTIO_GPU_CMD3D_SUBMIT_3D (0x0207)
#define VIRTIO_GPU_CMD3D_RESOURCE_MAP_BLOB (0x0208)
#define VIRTIO_GPU_CMD3D_RESOURCE_UNMAP_BLOB (0x0209)

#define VIRTIO_GPU_CMD_UPDATE_CURSOR (0x0300)
#define VIRTIO_GPU_CMD_MOVE_CURSOR (0x0301)

#define VIRTIO_GPU_RESPONSE_OK_NODATA (0x1100)
#define VIRTIO_GPU_RESPONSE_OK_DISPLAY_INFO (0x1101)
#define VIRTIO_GPU_RESPONSE_OK_CAPSET_INFO (0x1102)
#define VIRTIO_GPU_RESPONSE_OK_CAPSET (0x1103)
#define VIRTIO_GPU_RESPONSE_OK_MONITOR_IDENT (0x1104)
#define VIRTIO_GPU_RESPONSE_OK_RESOURCE_PLANE_INFO (0x1105)
#define VIRTIO_GPU_RESPONSE_OK_MAP_INFO (0x1106)

#define VIRTIO_GPU_RESPONSE_ERROR_UNKNOWN (0x1200)
#define VIRTIO_GPU_RESPONSE_ERROR_OUT_OF_VRAM (0x1201)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_SCANOUT (0x1202)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_RESOURCE_ID (0x1203)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_CONTEXT_ID (0x1204)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_PARAMETER (0x1205)

#define VIRTIO_GPU_MAX_MEMORY_DESC_COUNT (64)
#define VIRTIO_GPU_MAX_COMMAND_COUNT (64)
#define VIRTIO_GPU_MAX_RESPONSE_COUNT (64)

#define VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT (0x0001)
#define VIRTIO_GPU_MEMORY_DESC_FLAG_WRITE (0x0002)
#define VIRTIO_GPU_MEMORY_DESC_FLAG_INDIRECT (0x0004)

#define VIRTIO_GPU_COMMAND_FLAG_FENCE (1<<0)
#define VIRTIO_GPU_COMMAND_FLAG_RING_INDEX (1<<1)

#define VIRTIO_GPU_MAX_SCANOUT_COUNT (16)
#define VIRTIO_GPU_MAX_CMD_CONTEXT_COUNT (16384)
#define VIRTIO_GPU_MAX_RESOURCE_COUNT (16384)
#define VIRTIO_GPU_MAX_CONTEXT_COUNT (8192)
#define VIRTIO_GPU_MAX_OBJECT_COUNT (8192)

#define VIRTIO_GPU_RESPONSE_RING_FLAG_NO_NOTIFY (0x01)

#define VIRTIO_GPU_RESOURCE_TYPE_INVALID (0x00)
#define VIRTIO_GPU_RESOURCE_TYPE_2D (0x01)
#define VIRTIO_GPU_RESOURCE_TYPE_3D (0x02)

#define VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP (1<<0)
#define VIRTIO_GPU_RESOURCE_FLAG_MAP_PERSISTENT (1<<1)
#define VIRTIO_GPU_RESOURCE_FLAG_MAP_COHERENT (1<<2)

#define VIRTIO_GPU_RESOURCE_BLOB_MEM_FLAG_GUEST (0x01)
#define VIRTIO_GPU_RESOURCE_BLOB_MEM_FLAG_HOST (0x02)
#define VIRTIO_GPU_RESOURCE_BLOB_MEM_FLAG_HOST_GUEST (0x03)

#define VIRTIO_GPU_RESOURCE_BLOB_MAP_FLAG_USE_MAPPABLE (0x01)
#define VIRTIO_GPU_RESOURCE_BLOB_MAP_FLAG_USE_SHAREABLE (0x02)
#define VIRTIO_GPU_RESOURCE_BLOB_MAP_FLAG_USE_CROSS_DEVICE (0x04)

#define VIRTIO_GPU_GL_OBJECT_TYPE_NULL (0x00)
#define VIRTIO_GPU_GL_OBJECT_TYPE_BLEND (0x01)
#define VIRTIO_GPU_GL_OBJECT_TYPE_RASTERIZER (0x02)
#define VIRTIO_GPU_GL_OBJECT_TYPE_DSA (0x03)
#define VIRTIO_GPU_GL_OBJECT_TYPE_SHADER (0x04)
#define VIRTIO_GPU_GL_OBJECT_TYPE_VERTEX_ELEMENT_LIST (0x05)
#define VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_VIEW (0x06)
#define VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_STATE (0x07)
#define VIRTIO_GPU_GL_OBJECT_TYPE_SURFACE (0x08)
#define VIRTIO_GPU_GL_OBJECT_TYPE_QUERY (0x09)
#define VIRTIO_GPU_GL_OBJECT_TYPE_STREAMOUT_TARGET (0x0A)
#define VIRTIO_GPU_GL_OBJECT_TYPE_MSAA_SURFACE (0x0B)

#define VIRTIO_GPU_GL_BIND_TYPE_DEPTH_STENCIL (1<<0)
#define VIRTIO_GPU_GL_BIND_TYPE_RENDER_TARGET (1<<1)
#define VIRTIO_GPU_GL_BIND_TYPE_SAMPLER_VIEW (1<<3)
#define VIRTIO_GPU_GL_BIND_TYPE_VERTEX_BUFFER (1<<4)
#define VIRTIO_GPU_GL_BIND_TYPE_INDEX_BUFFER (1<<5)
#define VIRTIO_GPU_GL_BIND_TYPE_CONSTANT_BUFFER (1<<6)
#define VIRTIO_GPU_GL_BIND_TYPE_DISPLAY_TARGET (1<<7)
#define VIRTIO_GPU_GL_BIND_TYPE_COMMAND_ARGS (1<<8)
#define VIRTIO_GPU_GL_BIND_TYPE_STREAM_OUTPUT (1<<11)
#define VIRTIO_GPU_GL_BIND_TYPE_SHADER_BUFFER (1<<14)
#define VIRTIO_GPU_GL_BIND_TYPE_QUERY_BUFFER (1<<15)
#define VIRTIO_GPU_GL_BIND_TYPE_CURSOR (1<<16)
#define VIRTIO_GPU_GL_BIND_TYPE_CUSTOM (1<<17)
#define VIRTIO_GPU_GL_BIND_TYPE_SCANOUT (1<<18)

#define VIRTIO_GPU_GL_CMD_NOP (0x00)
#define VIRTIO_GPU_GL_CMD_CREATE_OBJECT (0x01)
#define VIRTIO_GPU_GL_CMD_BIND_OBJECT (0x02)
#define VIRTIO_GPU_GL_CMD_DELETE_OBJECT (0x03)
#define VIRTIO_GPU_GL_CMD_SET_VIEWPORT_STATE_LIST (0x04)
#define VIRTIO_GPU_GL_CMD_SET_FRAMEBUFFER_STATE_LIST (0x05)
#define VIRTIO_GPU_GL_CMD_SET_VERTEX_BUFFER_LIST (0x06)
#define VIRTIO_GPU_GL_CMD_CLEAR (0x07)
#define VIRTIO_GPU_GL_CMD_DRAW_VBO (0x08)
#define VIRTIO_GPU_GL_CMD_RESOURCE_INLINE_WRITE (0x09)
#define VIRTIO_GPU_GL_CMD_SET_SAMPLE_VIEWS (0x0A)
#define VIRTIO_GPU_GL_CMD_SET_INDEX_BUFFER (0x0B)
#define VIRTIO_GPU_GL_CMD_SET_CONSTANT_BUFFER (0x0C)
#define VIRTIO_GPU_GL_CMD_SET_STENCIL_REFERENCE (0x0D)
#define VIRTIO_GPU_GL_CMD_SET_BLEND_COLOR (0x0E)
#define VIRTIO_GPU_GL_CMD_SET_SCISSOR_STATE_LIST (0x0F)
#define VIRTIO_GPU_GL_CMD_BLIT (0x10)
#define VIRTIO_GPU_GL_CMD_RESOURCE_COPY_REGION (0x11)
#define VIRTIO_GPU_GL_CMD_BIND_SAMPLER_STATES (0x12)
#define VIRTIO_GPU_GL_CMD_BEGIN_QUERY (0x13)
#define VIRTIO_GPU_GL_CMD_END_QUERY (0x14)
#define VIRTIO_GPU_GL_CMD_GET_QUERY_RESULT (0x15)
#define VIRTIO_GPU_GL_CMD_SET_POLYGON_STIPPLE (0x16)
#define VIRTIO_GPU_GL_CMD_SET_CLIP_STATE (0x17)
#define VIRTIO_GPU_GL_CMD_SET_SAMPLE_MASK (0x18)
#define VIRTIO_GPU_GL_CMD_SET_STREAMOUT_TARGETS (0x19)
#define VIRTIO_GPU_GL_CMD_SET_UNDER_CONDITION (0x1A)
#define VIRTIO_GPU_GL_CMD_SET_UNIFORM_BUFFER (0x1B)

#define VIRTIO_GPU_GL_CMD_SET_SUB_CONTEXT (0x1C)
#define VIRTIO_GPU_GL_CMD_CREATE_SUB_CONTEXT (0x1D)
#define VIRTIO_GPU_GL_CMD_DELETE_SUB_CONTEXT (0x1E)
#define VIRTIO_GPU_GL_CMD_BIND_SHADER (0x1F)
#define VIRTIO_GPU_GL_CMD_SET_TESS_STATE (0x20)
#define VIRTIO_GPU_GL_CMD_SET_MIN_SAMPLES (0x21)
#define VIRTIO_GPU_GL_CMD_SET_SHADER_BUFFERS (0x22)
#define VIRTIO_GPU_GL_CMD_SET_SHADER_IMAGES (0x23)
#define VIRTIO_GPU_GL_CMD_MEMORY_BARRIER (0x24)
#define VIRTIO_GPU_GL_CMD_LAUNCH_GRID (0x25)

#define VIRTIO_GPU_GL_CMD_SET_FRAMEBUFFER_STATE_NO_ATTACH (0x26)
#define VIRTIO_GPU_GL_CMD_TEXTURE_BARRIER (0x27)
#define VIRTIO_GPU_GL_CMD_SET_ATOMIC_BUFFRES (0x28)
#define VIRTIO_GPU_GL_CMD_SET_DEBUG_FLAGS (0x29)
#define VIRTIO_GPU_GL_CMD_GET_QUERY_RESULT_QB0 (0x2A)
#define VIRTIO_GPU_GL_CMD_TRANSFER3D (0x2B)
#define VIRTIO_GPU_GL_CMD_END_TRANSFERS (0x2C)
#define VIRTIO_GPU_GL_CMD_COPY_TRANSFER3D (0x2D)
#define VIRTIO_GPU_GL_CMD_SET_TWEAKS (0x2E)
#define VIRTIO_GPU_GL_CMD_CLEAR_TEXTURE (0x2F)
#define VIRTIO_GPU_GL_CMD_PIPE_RESOURCE_CREATE (0x30)
#define VIRTIO_GPU_GL_CMD_PIPE_RESOURCE_SET_TYPE (0x31)
#define VIRTIO_GPU_GL_CMD_GET_MEMORY_INFO (0x32)
#define VIRTIO_GPU_GL_CMD_EMIT_STRING_MARKER (0x33)
#define VIRTIO_GPU_GL_CMD_LINK_SHADER (0x34)

#define VIRTIO_GPU_GL_CMD_CREATE_VIDEO_CODEC (0x35)
#define VIRTIO_GPU_GL_CMD_DELETE_VIDEO_CODEC (0x36)
#define VIRTIO_GPU_GL_CMD_CREATE_VIDEO_BUFFER (0x37)
#define VIRTIO_GPU_GL_CMD_DELETE_VIDEO_BUFFER (0x38)
#define VIRTIO_GPU_GL_CMD_BEGIN_FRAME (0x39)
#define VIRTIO_GPU_GL_CMD_DECODE_MACROBLOCK (0x3A)
#define VIRTIO_GPU_GL_CMD_DECODE_BITSTREAM (0x3B)
#define VIRTIO_GPU_GL_CMD_ENCODE_BITSTREAM (0x3C)
#define VIRTIO_GPU_GL_CMD_END_FRAME (0x3D)

#define VIRTIO_GPU_GL_CMD_CLEAR_SURFACE (0x3E)
#define VIRTIO_GPU_GL_CMD_GET_PIPE_RESOURCE_LAYOUT (0x3F)

#define VIRTIO_GPU_GL_SHADER_TYPE_VERTEX (0x00)
#define VIRTIO_GPU_GL_SHADER_TYPE_FRAGMENT (0x01)
#define VIRTIO_GPU_GL_SHADER_TYPE_GEOMETRY (0x02)
#define VIRTIO_GPU_GL_SHADER_TYPE_TESS_CONTROL (0x03)
#define VIRTIO_GPU_GL_SHADER_TYPE_TESS_EVALUATE (0x04)
#define VIRTIO_GPU_GL_SHADER_TYPE_COMPUTE (0x05)

#define VIRTIO_GPU_PIXEL_FORMAT_INVALID (0x00)
#define VIRTIO_GPU_PIXEL_FORMAT_B8G8R8A8_UNORM (0x01)
#define VIRTIO_GPU_PIXEL_FORMAT_B8G8R8X8_UNORM (0x02)
#define VIRTIO_GPU_PIXEL_FORMAT_R8G8B8A8_UNORM (67)
#define VIRTIO_GPU_PIXEL_FORMAT_R32G32B32A32_FLOAT (31)
#define VIRTIO_GPU_PIXEL_FORMAT_R32G32B32_FLOAT (30)
#define VIRTIO_GPU_PIXEL_FORMAT_R32G32_FLOAT (29)

#define VIRTIO_GPU_GL_TARGET_BUFFER (0x00)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_1D (0x01)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_2D (0x02)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_3D (0x03)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_CUBE (0x04)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_RECT (0x05)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_1D_ARRAY (0x06)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_2D_ARRAY (0x07)
#define VIRTIO_GPU_GL_TARGET_TEXTURE_CUBE_ARRAY (0x08)

#define VIRTIO_GPU_GL_RASTERIZER_S0_FLATSHADE (1<<0)
#define VIRTIO_GPU_GL_RASTERIZER_S0_DEPTH_CLIP (1<<1)
#define VIRTIO_GPU_GL_RASTERIZER_S0_CLIP_HALFZ (1<<2)
#define VIRTIO_GPU_GL_RASTERIZER_S0_DISCARD (1<<3)
#define VIRTIO_GPU_GL_RASTERIZER_S0_FLATSHADE_FIRST (1<<4)
#define VIRTIO_GPU_GL_RASTERIZER_S0_LIGHT_TWOSIDE (1<<5)
#define VIRTIO_GPU_GL_RASTERIZER_S0_SPRITE_COORD_MODE (1<<6)
#define VIRTIO_GPU_GL_RASTERIZER_S0_POINT_QUAD_RASTERIZATION (1<<7)
#define VIRTIO_GPU_GL_RASTERIZER_S0_CULL_FACE_SHIFT (8)
#define VIRTIO_GPU_GL_RASTERIZER_S0_FILL_FRONT_SHIFT (10)
#define VIRTIO_GPU_GL_RASTERIZER_S0_FILL_BACK_SHIFT (12)
#define VIRTIO_GPU_GL_RASTERIZER_S0_SCISSOR (1<<14)
#define VIRTIO_GPU_GL_RASTERIZER_S0_FRONT_CCW (1<<15)
#define VIRTIO_GPU_GL_RASTERIZER_S0_CLAMP_VERTEX_COLOR (1<<16)
#define VIRTIO_GPU_GL_RASTERIZER_S0_CLAMP_FRAGMENT_COLOR (1<<17)
#define VIRTIO_GPU_GL_RASTERIZER_S0_OFFSET_LINE (1<<18)
#define VIRTIO_GPU_GL_RASTERIZER_S0_OFFSET_POINT (1<<19)
#define VIRTIO_GPU_GL_RASTERIZER_S0_OFFSET_TRI (1<<20)
#define VIRTIO_GPU_GL_RASTERIZER_S0_POLY_SMOOTH (1<<21)
#define VIRTIO_GPU_GL_RASTERIZER_S0_POLY_STIPPLE_ENABLE (1<<22)
#define VIRTIO_GPU_GL_RASTERIZER_S0_POINT_SMOOTH (1<<23)
#define VIRTIO_GPU_GL_RASTERIZER_S0_POINT_SIZE_PER_VERTEX (1<<24)
#define VIRTIO_GPU_GL_RASTERIZER_S0_MULTISAMPLE (1<<25)
#define VIRTIO_GPU_GL_RASTERIZER_S0_LINE_SMOOTH (1<<26)
#define VIRTIO_GPU_GL_RASTERIZER_S0_LINE_STIPPLE_ENABLE (1<<27)
#define VIRTIO_GPU_GL_RASTERIZER_S0_LINE_LAST_PIXEL (1<<28)
#define VIRTIO_GPU_GL_RASTERIZER_S0_HALF_PIXEL_CENTER (1<<29)
#define VIRTIO_GPU_GL_RASTERIZER_S0_BOTTOM_EDGE_RULE (1<<30)
#define VIRTIO_GPU_GL_RASTERIZER_S0_FORCE_PERSAMPLE_INTERP (1<<31)

#define VIRTIO_GPU_GL_RASTERIZER_S3_LINE_STIPPLE_PATTERN_SHIFT (0)
#define VIRTIO_GPU_GL_RASTERIZER_S3_LINE_STIPPLE_FACTOR_SHIFT (16)
#define VIRTIO_GPU_GL_RASTERIZER_S3_CLIP_PLANE_ENABLE_SHIFT (24)

#define VIRTIO_GPU_GL_FACE_INVALID (0x00)
#define VIRTIO_GPU_GL_FACE_FRONT (0x01)
#define VIRTIO_GPU_GL_FACE_BACK (0x02)
#define VIRTIO_GPU_GL_FACE_FRONT_AND_BACK (0x03)

#define VIRTIO_GPU_GL_POLYGON_MODE_FILL (0x00)
#define VIRTIO_GPU_GL_POLYGON_MODE_LINE (0x01)
#define VIRTIO_GPU_GL_POLYGON_MODE_POINT (0x02)
#define VIRTIO_GPU_GL_POLYGON_MODE_FILL_RECT (0x03)

#define VIRTIO_GPU_GL_DSA_S0_DEPTH_ENABLE (1<<0)
#define VIRTIO_GPU_GL_DSA_S0_DEPTH_WRITEMASK (1<<1)
#define VIRTIO_GPU_GL_DSA_S0_DEPTH_FUNC (2)
#define VIRTIO_GPU_GL_DSA_S0_ALPHA_ENABLE (1<<8)
#define VIRTIO_GPU_GL_DSA_S0_ALPHA_FUNC (9)

#define VIRTIO_GPU_GL_DSA_S1_STENCIL_ENABLE (1<<0)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_FUNC (1)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_FAIL (4)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_ZPASS (7)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_ZFAIL (10)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_VALUE (13)
#define VIRTIO_GPU_GL_DSA_S1_STENCIL_WRITEMASK (21)

#define VIRTIO_GPU_GL_FUNC_NEVER (0x00)
#define VIRTIO_GPU_GL_FUNC_LESS (0x01)
#define VIRTIO_GPU_GL_FUNC_EQUAL (0x02)
#define VIRTIO_GPU_GL_FUNC_LEQUAL (0x03)
#define VIRTIO_GPU_GL_FUNC_GREATER (0x04)
#define VIRTIO_GPU_GL_FUNC_NEQUAL (0x05)
#define VIRTIO_GPU_GL_FUNC_GEQUAL (0x06)
#define VIRTIO_GPU_GL_FUNC_ALWAYS (0x07)

#define VIRTIO_GPU_GL_BLEND_ADD (0x00)
#define VIRTIO_GPU_GL_BLEND_SUB (0x01)
#define VIRTIO_GPU_GL_BLEND_REVERSE_SUB (0x02)
#define VIRTIO_GPU_GL_BLEND_MIN (0x03)
#define VIRTIO_GPU_GL_BLEND_MAX (0x04)

#define VIRTIO_GPU_GL_BLENDFACTOR_ONE (0x01)
#define VIRTIO_GPU_GL_BLENDFACTOR_SRC_COLOR (0x02)
#define VIRTIO_GPU_GL_BLENDFACTOR_SRC_ALPHA (0x03)
#define VIRTIO_GPU_GL_BLENDFACTOR_DEST_ALPHA (0x04)
#define VIRTIO_GPU_GL_BLENDFACTOR_DEST_COLOR (0x05)
#define VIRTIO_GPU_GL_BLENDFACTOR_SRC_ALPHA_SATURATE (0x06)
#define VIRTIO_GPU_GL_BLENDFACTOR_CONST_COLOR (0x07)
#define VIRTIO_GPU_GL_BLENDFACTOR_CONST_ALPHA (0x08)
#define VIRTIO_GPU_GL_BLENDFACTOR_SRC1_COLOR (0x09)
#define VIRTIO_GPU_GL_BLENDFACTOR_SRC1_ALPHA (0x0A)
#define VIRTIO_GPU_GL_BLENDFACTOR_ZERO (0x11)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_SRC_COLOR (0x12)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_SRC_ALPHA (0x13)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_DEST_ALPHA (0x14)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_DEST_COLOR (0x15)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_CONST_COLOR (0x17)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_CONST_ALPHA (0x18)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_SRC1_COLOR (0x19)
#define VIRTIO_GPU_GL_BLENDFACTOR_INV_SRC1_ALPHA (0x1A)

#define VIRTIO_GPU_GL_BLEND_INDEPENDENT_BLEND_ENABLE (1<<0)
#define VIRTIO_GPU_GL_BLEND_LOGICOP_ENABLE (1<<1)
#define VIRTIO_GPU_GL_BLEND_LOGICOP_FUNC_SHIFT (2)
#define VIRTIO_GPU_GL_BLEND_DITHER (1<<6)
#define VIRTIO_GPU_GL_BLEND_ALPHA_TO_COVERAGE (1<<7)
#define VIRTIO_GPU_GL_BLEND_ALPHA_TO_ONE (1<<8)

#define VIRTIO_GPU_GL_COLOR_MASK_INVALID (0x00)
#define VIRTIO_GPU_GL_COLOR_MASK_R (0x01)
#define VIRTIO_GPU_GL_COLOR_MASK_G (0x02)
#define VIRTIO_GPU_GL_COLOR_MASK_B (0x04)
#define VIRTIO_GPU_GL_COLOR_MASK_A (0x08)
#define VIRTIO_GPU_GL_COLOR_MASK_RGBA (0x0F)

#define VIRTIO_GPU_GL_PRIMITIVE_POINTS (0x00)
#define VIRTIO_GPU_GL_PRIMITIVE_LINES (0x01)
#define VIRTIO_GPU_GL_PRIMITIVE_LINES_LOOP (0x02)
#define VIRTIO_GPU_GL_PRIMITIVE_LINES_STRIP (0x03)
#define VIRTIO_GPU_GL_PRIMITIVE_TRIANGLES (0x04)
#define VIRTIO_GPU_GL_PRIMITIVE_TRIANGLES_STRIP (0x05)
#define VIRTIO_GPU_GL_PRIMITIVE_TRIANGLES_FAN (0x06)
#define VIRTIO_GPU_GL_PRIMITIVE_QUADS (0x07)
#define VIRTIO_GPU_GL_PRIMITIVE_QUADS_STRIP (0x08)
#define VIRTIO_GPU_GL_PRIMITIVE_POLYGON (0x09)
#define VIRTIO_GPU_GL_PRIMITIVE_LINES_ADJACENCY (0x0A)
#define VIRTIO_GPU_GL_PRIMITIVE_LINE_STRIP_ADJACENCY (0x0B)

#define VIRTIO_GPU_GL_TRANSFER_READ (1<<0)
#define VIRTIO_GPU_GL_TRANSFER_WRITE (1<<1)
#define VIRTIO_GPU_GL_TRANSFER_MAP_DIRECTLY (1<<2)
#define VIRTIO_GPU_GL_TRANSFER_DISCARD_RANGE (1<<8)
#define VIRTIO_GPU_GL_TRANSFER_NO_BLOCK (1<<9)
#define VIRTIO_GPU_GL_TRANSFER_UNSYNCED (1<<10)
#define VIRTIO_GPU_GL_TRANSFER_FLUSH_EXPLICIT (1<<11)
#define VIRTIO_GPU_GL_TRANSFER_DISCARD_FULL_RESOURCE (1<<12)
#define VIRTIO_GPU_GL_TRANSFER_PERSISTENT (1<<13)
#define VIRTIO_GPU_GL_TRANSFER_COHERENT (1<<14)

#define VIRTIO_GPU_GL_TRANSFER_DIRECTION_H2D (0x01)
#define VIRTIO_GPU_GL_TRANSFER_DIRECTION_D2H (0x02)

struct virtio_gpu_pcie_config_cap{
	struct pcie_cap_link link;
	uint8_t cap_size;
	uint8_t config_type;
	uint8_t bar;
	uint8_t padding0[3];
	uint32_t bar_offset;
	uint32_t register_block_size;
}__attribute__((packed));
struct virtio_gpu_pcie_config_cap_notify{
	struct virtio_gpu_pcie_config_cap header;
	uint32_t notify_offset_multiplier;
}__attribute__((packed));
struct virtio_gpu_pcie_config_cap_shared_memory{
	struct virtio_gpu_pcie_config_cap header;
	uint32_t offset_high;
	uint32_t length_high;
}__attribute__((packed));
struct virtio_gpu_features_legacy{
	uint32_t virgl_support:1;
	uint32_t edid_metadata_support:1;
	uint32_t resource_uuid_support:1;
	uint32_t host_memory_blob_support:1;
	uint32_t context_init_support:1;
	uint32_t reserved0:19;
	uint32_t notify_on_empty:1;
	uint32_t reserved1:2;
	uint32_t legacy_descriptor_layout:1;
	uint32_t indirect_descriptor_support:1;
	uint32_t interupt_suppression_support:1;
	uint32_t reserved2:2;
}__attribute__((packed));
struct virtio_gpu_features_modern{
	uint32_t version_1:1;
	uint32_t iommu_protected:1;
	uint32_t ring_packed:1;
	uint32_t ordered_descriptors:1;
	uint32_t same_memory_model:1;
	uint32_t single_root_io_virtualization:1;
	uint32_t notification_data:1;
	uint32_t notification_config_data:1;
	uint32_t ring_reset_support:1;
	uint32_t reserved0:23;
}__attribute__((packed));
struct virtio_gpu_device_status{
	uint8_t acknowledge:1;
	uint8_t driver:1;
	uint8_t driver_ok:1;
	uint8_t features_ok:1;
	uint8_t reserved0:2;
	uint8_t reset_required:1;
	uint8_t failed:1;
}__attribute__((packed));
struct virtio_gpu_base_registers{
	volatile uint32_t device_feature_select;
	volatile uint32_t device_feature;
	volatile uint32_t driver_feature_select;
	volatile uint32_t driver_feature;
	volatile uint16_t msix_config;
	volatile uint16_t queue_count;
	volatile struct virtio_gpu_device_status device_status;	
	volatile uint8_t config_generation;
	volatile uint16_t queue_select;
	volatile uint16_t queue_size;	
	volatile uint16_t queue_msix_vector;
	volatile uint16_t queue_enable;
	volatile uint16_t queue_notify_id;	
	volatile uint32_t queue_memory_desc_list_base_low;
	volatile uint32_t queue_memory_desc_list_base_high;
	volatile uint32_t queue_command_ring_base_low;
	volatile uint32_t queue_command_ring_base_high;
	volatile uint32_t queue_response_ring_base_low;
	volatile uint32_t queue_response_ring_base_high;
}__attribute__((packed));
struct virtio_gpu_interrupt_registers{
	volatile uint8_t isr_status;
}__attribute__((packed));
struct virtio_gpu_device_registers{
	volatile uint32_t events_read;
	volatile uint32_t events_clear;
	volatile uint32_t scanout_count;
	volatile uint32_t capset_count;
}__attribute__((packed));
struct virtio_gpu_pcie_registers{
		
}__attribute__((packed));
struct virtio_gpu_memory_desc{
	uint64_t physical_address;
	uint32_t length;
	uint16_t flags;
	uint16_t next;
}__attribute__((packed));
struct virtio_gpu_command_header{
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t context_id;
	uint32_t ring_index;
}__attribute__((packed));
struct virtio_gpu_response_header{
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t context_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_command_list_entry{
	uint16_t index;
}__attribute__((packed));
struct virtio_gpu_response_list_entry{
	uint32_t index;
	uint32_t length;
}__attribute__((packed));
struct virtio_gpu_memory_entry{
	uint64_t physicalAddress;
	uint32_t length;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_rect{
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
}__attribute__((packed));
struct virtio_gpu_box{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
}__attribute__((packed));
struct virtio_gpu_create_resource_2d_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t format;
	uint32_t width;
	uint32_t height;
}__attribute__((packed));
struct virtio_gpu_create_resource_3d_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t target;
	uint32_t format;
	uint32_t bind;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t array_size;
	uint32_t last_level;
	uint32_t nr_samples;
	uint32_t flags;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_delete_resource_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_attach_backing_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t memory_entry_count;
	struct virtio_gpu_memory_entry memoryEntryList[];
}__attribute__((packed));
struct virtio_gpu_create_resource_blob_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t mem_flags;
	uint32_t map_flags;
	uint64_t blob_id;
	uint64_t size;
	uint32_t memory_entry_count;
	struct virtio_gpu_memory_entry memoryEntryList[];
}__attribute__((packed));
struct virtio_gpu_set_scanout_command{
	struct virtio_gpu_command_header commandHeader;
	struct virtio_gpu_rect rect;
	uint32_t scanout_id;
	uint32_t resource_id;
}__attribute__((packed));
struct virtio_gpu_transfer_to_host_2d_command{
	struct virtio_gpu_command_header commandHeader;
	struct virtio_gpu_rect rect;
	uint64_t offset;
	uint32_t resource_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_transfer_to_host_3d_command{
	struct virtio_gpu_command_header commandHeader;
	struct virtio_gpu_box box;
	uint64_t offset;
	uint32_t resource_id;
	uint32_t level;
	uint32_t stride;
	uint32_t layer_stride;
}__attribute__((packed));
struct virtio_gpu_transfer_from_host_3d_command{
	struct virtio_gpu_command_header commandHeader;
	struct virtio_gpu_box box;
	uint64_t offset;
	uint32_t resource_id;
	uint32_t level;
	uint32_t stride;
	uint32_t layer_stride;
}__attribute__((packed));
struct virtio_gpu_resource_flush_command{
	struct virtio_gpu_command_header commandHeader;
	struct virtio_gpu_rect rect;
	uint32_t resource_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_create_context_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t debug_name_length;
	uint32_t padding0;
	unsigned char debug_name[64];
}__attribute__((packed));
struct virtio_gpu_delete_context_command{
	struct virtio_gpu_command_header commandHeader;
}__attribute__((packed));
struct virtio_gpu_attach_context_resource_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_detach_context_resource_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t resource_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_submit_3d_command{
	struct virtio_gpu_command_header commandHeader;
	uint32_t size;
	uint32_t padding0;
	unsigned char command_data[];
}__attribute__((packed));
struct virtio_gpu_gl_command_header{
	uint8_t opcode;
	uint8_t object_type;
	uint16_t length;
}__attribute__((packed));
struct virtio_gpu_gl_create_surface_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t resource_id;
	uint32_t format;
	union{
		struct{
			uint32_t level;
			uint32_t first_layer;
		}texture;
		struct{
			uint32_t first_element;
			uint32_t last_element;
		}buffer;
	};
}__attribute__((packed));
struct virtio_gpu_gl_vertex_element{
	uint32_t src_offset;
	uint32_t instance_divisor;
	uint32_t vertex_buffer_index;
	uint32_t src_format;
}__attribute__((packed));
struct virtio_gpu_gl_create_vertex_element_list_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	struct virtio_gpu_gl_vertex_element vertex_element_list[];
}__attribute__((packed));
struct virtio_gpu_gl_rasterizer_state{
	uint32_t s0;
	float point_size;
	uint32_t sprite_coord_enable;
	uint32_t s3;
	float line_width;
	float offset_units;
	float offset_scale;
	float offset_clamp;
}__attribute__((packed));
struct virtio_gpu_gl_create_rasterizer_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	struct virtio_gpu_gl_rasterizer_state rasterizer_state;
}__attribute__((packed));
struct virtio_gpu_gl_dsa_state{
	uint32_t s0;
	uint32_t stencil_front;
	uint32_t stencil_back;
	uint32_t s3;
}__attribute__((packed));
struct virtio_gpu_gl_create_dsa_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	struct virtio_gpu_gl_dsa_state dsa_state;
}__attribute__((packed));
struct virtio_gpu_gl_blend_state{
	uint32_t blend_enable:1;
	uint32_t rgb_func:3;
	uint32_t rgb_src_factor:5;
	uint32_t rgb_dest_factor:5;
	uint32_t alpha_func:3;
	uint32_t alpha_src_factor:5;
	uint32_t alpha_dest_factor:5;
	uint32_t color_mask:4;
}__attribute__((packed));
struct virtio_gpu_gl_create_blend_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t s0;
	uint32_t s1;
	struct virtio_gpu_gl_blend_state blend_state_list[];
}__attribute__((packed));
struct virtio_gpu_gl_stream_output{
	uint32_t register_index;
	uint32_t start_component;
	uint32_t component_count;
	uint32_t output_buffer;
	uint32_t dst_offset;
	uint32_t stream;
}__attribute__((packed));
struct virtio_gpu_gl_shader_object_segment_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t offset_length;
	unsigned char segment_data[];
}__attribute__((packed));
struct virtio_gpu_gl_create_shader_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t shader_type;
	uint32_t offset_length;
	uint32_t token_count;
	uint32_t shader_stream_output_count;
	unsigned char segment_data[];
}__attribute__((packed));
struct virtio_gpu_gl_create_shader_object_so_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t shader_type;
	uint32_t offset_length;
	uint32_t token_count;
	uint32_t shader_stream_output_count;
	uint32_t shader_stream_output_stride[4];
	struct virtio_gpu_gl_stream_output stream_output_list[];
}__attribute__((packed));
struct virtio_gpu_gl_bind_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
}__attribute__((packed));
struct virtio_gpu_gl_bind_shader_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
	uint32_t shader_type;
}__attribute__((packed));
struct virtio_gpu_gl_delete_object_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t object_id;
}__attribute__((packed));
struct virtio_gpu_gl_viewport_state{
	struct fvec3_32 scale;
	struct fvec3_32 translate;
}__attribute__((packed));
struct virtio_gpu_gl_set_viewport_state_list_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t start_slot;
	struct virtio_gpu_gl_viewport_state viewport_state_list[];
}__attribute__((packed));
struct virtio_gpu_gl_set_framebuffer_state_list_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t color_buffer_count;
	uint32_t depth_stencil_handle;
	uint32_t color_buffer_handle_list[8];
}__attribute__((packed));
struct virtio_gpu_gl_scissor_state{
	uint16_t minX;
	uint16_t minY;
	uint16_t maxX;
	uint16_t maxY;
}__attribute__((packed));
struct virtio_gpu_gl_set_scissor_state_list_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t start_slot;
	struct virtio_gpu_gl_scissor_state scissor_state_list[];
}__attribute__((packed));
struct virtio_gpu_gl_vertex_buffer{
	uint32_t stride;
	uint32_t offset;
	uint32_t resource_id;
}__attribute__((packed));
struct virtio_gpu_gl_set_vertex_buffer_list_command{
	struct virtio_gpu_gl_command_header header;
	struct virtio_gpu_gl_vertex_buffer vertex_buffer_list[];
}__attribute__((packed));
struct virtio_gpu_gl_draw_vbo_command_info{
	uint32_t start;
	uint32_t count;
	uint32_t mode;
	uint32_t indexed;
	uint32_t instance_count;
	uint32_t index_bias;
	uint32_t start_instance;
	uint32_t primitive_restart;
	uint32_t restart_index;
	uint32_t min_index;
	uint32_t max_index;
	uint32_t stream_output_count;
}__attribute__((packed));
struct virtio_gpu_gl_draw_vbo_command{
	struct virtio_gpu_gl_command_header header;
	struct virtio_gpu_gl_draw_vbo_command_info commandInfo;
}__attribute__((packed));
struct virtio_gpu_gl_clear_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t buffers;
	struct fvec4_32 color;
	double depth;
	uint32_t stencil;	
}__attribute__((packed));
struct virtio_gpu_gl_transfer_3d_command{
	struct virtio_gpu_gl_command_header header;
	uint32_t resource_id;
	uint32_t level;
	uint32_t transfer_type;
	uint32_t stride;
	uint32_t layer_stride;
	struct virtio_gpu_box box_rect;
	uint32_t offset;
	uint32_t direction;
}__attribute__((packed));
struct virtio_gpu_scanout_info{
	struct virtio_gpu_rect resolution;
	uint32_t enabled;
	uint32_t flags;
}__attribute__((packed));
struct virtio_gpu_display_info_response{
	struct virtio_gpu_response_header responseHeader;
	struct virtio_gpu_scanout_info scanoutList[VIRTIO_GPU_MAX_SCANOUT_COUNT];
}__attribute__((packed));
struct virtio_gpu_command_ring{
	uint16_t flags;
	uint16_t index;
	struct virtio_gpu_command_list_entry commandList[VIRTIO_GPU_MAX_COMMAND_COUNT];	
	uint16_t used_event;
}__attribute__((packed));
struct virtio_gpu_response_ring{
	uint16_t flags;
	uint16_t index;
	struct virtio_gpu_response_list_entry responseList[VIRTIO_GPU_MAX_RESPONSE_COUNT];
}__attribute__((packed));
struct virtio_gpu_memory_desc_info{
	struct virtio_gpu_queue_info* pQueueInfo;
	uint64_t memoryDescIndex;
	volatile struct virtio_gpu_memory_desc* pMemoryDesc;
};
struct virtio_gpu_response_desc{
	struct virtio_gpu_queue_info* pQueueInfo;
	uint64_t completionEntryIndex;
	uint8_t responseComplete;
};
struct virtio_gpu_command_desc{
	struct virtio_gpu_queue_info* pQueueInfo;
	uint64_t commandIndex;
	unsigned char* pCommandBuffer;
	uint64_t pCommandBuffer_phys;
	uint64_t commandBufferSize;	
	unsigned char* pResponseBuffer;
	uint64_t pResponseBuffer_phys;
	uint64_t responseBufferSize;	
	struct virtio_gpu_response_desc responseDesc;
};
struct virtio_gpu_vertex_triangle{
	struct fvec4_32 position;
	struct fvec4_32 color;
}__attribute__((packed));
struct virtio_gpu_vertex_buffer_triangle{
	struct virtio_gpu_vertex_triangle vertex_list[3];
}__attribute__((packed));
struct virtio_gpu_vertex_buffer_quad{
	struct virtio_gpu_vertex_triangle vertex_list[6];
}__attribute__((packed));
struct virtio_gpu_create_shader_info{
	unsigned char* pShaderCode;
	uint64_t shaderCodeSize;
	uint64_t tokenCount;
	struct virtio_gpu_gl_stream_output* pStreamOutputList;
	uint64_t streamOutputCount;
	uint64_t shaderType;
};
struct virtio_gpu_surface_object_desc{
	struct virtio_gpu_object_desc* pObjectDesc;
};
struct virtio_gpu_shader_object_desc{
	struct virtio_gpu_object_desc* pObjectDesc;
	struct virtio_gpu_object_desc* pSurfaceObjectDesc;
	uint64_t shaderType;
};
struct virtio_gpu_object_desc{
	uint64_t objectId;
	uint8_t objectType;
	struct virtio_gpu_context_desc* pContextDesc;
	struct virtio_gpu_resource_desc* pResourceDesc;
	unsigned char* pExtra;
};
struct virtio_gpu_resource_desc{
	uint64_t resourceId;
	struct virtio_gpu_context_desc* pContextDesc;
	unsigned char* pBuffer;
	uint64_t bufferSize;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t bind;
	uint32_t format;
	uint32_t flags;
	uint8_t resourceType;
}; 
struct virtio_gpu_resource_blob_info{
	uint64_t resourceId;
	uint64_t memFlags;
	uint64_t mapFlags;
	unsigned char* pBlobBuffer;
	uint64_t blobBufferSize;
};
struct virtio_gpu_resource_blob_desc{
	uint64_t resourceBlobId;
	struct virtio_gpu_resource_blob_info resourceBlobInfo;
};
struct virtio_gpu_context_desc{
	uint64_t contextId;
};
struct virtio_gpu_cmd_context_desc{
	uint64_t cmdContextId;
	uint64_t commandListSize;
	uint64_t currentCommandOffset;
	uint64_t commandCount;
	struct virtio_gpu_submit_3d_command* pCommandBuffer;
	uint64_t commandBufferSize;
};
struct virtio_gpu_create_resource_info{
	struct virtio_gpu_context_desc* pContextDesc;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t target;
	uint32_t bind;
	uint32_t depth;
	uint32_t arraySize;
	uint32_t flags;
	unsigned char* pBackingBuffer;
	uint64_t backingBufferSize;
	uint8_t resourceType;
};
struct virtio_gpu_create_resource_blob_info{
	struct virtio_gpu_resource_desc* pResourceDesc;
	uint32_t memFlags;
	uint32_t mapFlags;
	unsigned char* pBlobBuffer;
	uint64_t blobBufferSize;
};
struct virtio_gpu_alloc_cmd_info{
	unsigned char* pCommandBuffer;
	uint64_t commandBufferSize;	
	unsigned char* pBuffer;
	uint64_t bufferSize;
	unsigned char* pResponseBuffer;
	uint64_t responseBufferSize;
};
struct virtio_gpu_queue_info{
	struct virtio_gpu_command_desc** ppCommandDescList;
	uint64_t pCommandDescListSize;
	struct virtio_gpu_response_desc** ppResponseDescList;
	uint64_t pResponseDescListSize;	
	volatile struct virtio_gpu_memory_desc* pMemoryDescList;
	volatile struct virtio_gpu_command_ring* pCommandRing;
	volatile struct virtio_gpu_response_ring* pResponseRing;	
	uint64_t pMemoryDescList_phys;	
	uint64_t pCommandRing_phys;
	uint64_t pResponseRing_phys;
	uint64_t maxMemoryDescCount;
	uint64_t maxCommandCount;
	uint64_t maxResponseCount;
	uint64_t memoryDescCount;
	uint64_t currentCommand;
	uint64_t currentResponse;	
	uint64_t oldResponseRingIndex;	
	uint8_t msixVector;
	uint8_t isrVector;
	uint16_t notifyId;
	uint8_t pollingEnabled;
}__attribute__((packed));
struct virtio_gpu_isr_mapping_table_entry{
	struct virtio_gpu_queue_info* pQueueInfo;
};
struct virtio_gpu_monitor_desc{
	uint64_t scanoutId;
	struct virtio_gpu_scanout_info scanoutInfo;
	uint64_t surfaceObjectId;
	uint64_t contextId;
	uint64_t resourceId;
};
struct virtio_gpu_info{
	struct pcie_location location;
	volatile struct virtio_gpu_base_registers* pBaseRegisters;
	volatile uint32_t* pNotifyRegisters;
	uint32_t notifyOffsetMultiplier;
	volatile struct virtio_gpu_interrupt_registers* pInterruptRegisters;
	volatile struct virtio_gpu_device_registers* pDeviceRegisters;
	volatile struct virtio_gpu_pcie_registers* pPcieRegisters;

	volatile struct pcie_msix_msg_ctrl* pMsgControl;	
	uint64_t queueCount;	
	struct virtio_gpu_isr_mapping_table_entry* pIsrMappingTable;
	uint64_t isrMappingTableSize;	
	struct subsystem_desc* pCmdContextSubsystemDesc;
	struct subsystem_desc* pObjectSubsystemDesc;
	struct virtio_gpu_queue_info controlQueueInfo;
	uint64_t driverId;
	struct gpu_driver_vtable driverVtable;
	struct gpu_driver_info driverInfo;
	uint64_t gpuId;
	uint8_t virglSupport;
	uint8_t panicMode;
};
typedef int(*virtioGpuPushCmdFunc)(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_cmd_info_header* pCommandInfo);
typedef int(*virtioGpuSubsystemCreateObjectFunc)(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_init(void);
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo);
int virtio_gpu_disable_legacy_vga(void);
int virtio_gpu_init_isr_mapping_table(void);
int virtio_gpu_deinit_isr_mapping_table(void);
int virtio_gpu_init_cmd_context_subsystem(void);
int virtio_gpu_deinit_cmd_context_subystem();
int virtio_gpu_init_object_subsystem(void);
int virtio_gpu_deinit_object_subsystem(void);
int virtio_gpu_get_response_type_name(uint16_t responseType, const unsigned char** ppResponseTypeName);
int virtio_gpu_reset(void);
int virtio_gpu_acknowledge(void);
int virtio_gpu_validate_driver(void);
int virtio_gpu_sync(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box boxRect, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_commit(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box boxRect, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_push(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_get_display_info(struct virtio_gpu_display_info_response* pDisplayInfo);
int virtio_gpu_create_resource_2d(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_create_resource_3d(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_create_resource(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_delete_resource(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_attach_resource_backing(struct virtio_gpu_resource_desc* pResourceDesc, unsigned char* pBuffer, uint64_t bufferSize, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_create_resource_blob(struct virtio_gpu_create_resource_blob_info createResourceBlobInfo, uint64_t resourceBlobId, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_set_scanout(uint32_t scanoutId, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_transfer_h2d_2d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_transfer_h2d_3d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box box, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_transfer_d2h_3d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box box, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_resource_flush(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_create_context(uint64_t contextId, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_delete_context(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_context_attach_resource(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_context_detach_resource(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_submit(struct virtio_gpu_submit_3d_command* pCommandBuffer, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_surface_object(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_vertex_element_list_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint32_t vertexElementCount, struct virtio_gpu_gl_vertex_element* pVertexElementList, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_set_vertex_buffer_list(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint64_t vertexBufferCount, struct virtio_gpu_gl_vertex_buffer* pVertexBufferList, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_rasterizer_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_gl_rasterizer_state rasterizerState, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_dsa_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_gl_dsa_state dsaState, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_blend_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint32_t s0, uint32_t s1, uint32_t blendStateCount, struct virtio_gpu_gl_blend_state* pBlendStateList, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_set_framebuffer_state_list(struct virtio_gpu_context_desc* pContextDesc, uint32_t colorBufferCount, uint32_t depthStencilHandle, uint32_t* pColorBufferHandleList, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_set_viewport_state_list(struct virtio_gpu_context_desc* pContextDesc, uint32_t startSlot, uint32_t viewportStateCount, struct virtio_gpu_gl_viewport_state* pViewportStateList, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_set_scissor_state_list(struct virtio_gpu_context_desc* pContextDesc, uint32_t startSlot, uint32_t scissorStateCount, struct virtio_gpu_gl_scissor_state* pScissorStateList, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_create_shader_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_create_shader_info createShaderInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_delete_shader_object(struct virtio_gpu_object_desc* pShaderObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_delete_object(struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_bind_object(struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_clear(struct virtio_gpu_context_desc* pContextDesc, uint32_t buffers, struct fvec4_32 color, double depth, uint32_t stencil, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_draw_vbo(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_gl_draw_vbo_command_info commandInfo, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_transfer_3d(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_gl_transfer_3d_command command, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_gl_write_delete_object(struct virtio_gpu_gl_delete_object_command* pCommand, uint32_t objectId);
int virtio_gpu_gl_write_bind_object(struct virtio_gpu_gl_bind_object_command* pCommand, uint32_t objectId, uint32_t objectType);
int virtio_gpu_gl_write_bind_shader_object(struct virtio_gpu_gl_bind_shader_object_command* pCommand, uint32_t objectId, uint32_t objectType);
int virtio_gpu_gl_write_create_surface(struct virtio_gpu_gl_create_surface_object_command* pCommand, uint32_t objectId, uint32_t resourceId, uint32_t format, uint32_t dword0, uint32_t dword1);
int virtio_gpu_gl_write_create_vertex_element_list(struct virtio_gpu_gl_create_vertex_element_list_object_command* pCommand, uint32_t objectId, uint32_t vertexElementCount, struct virtio_gpu_gl_vertex_element* pVertexElementList);
int virtio_gpu_gl_write_set_vertex_buffer_list(struct virtio_gpu_gl_set_vertex_buffer_list_command* pCommand, uint64_t vertexBufferCount, struct virtio_gpu_gl_vertex_buffer* pVertexBufferList);
int virtio_gpu_gl_write_create_rasterizer_object(struct virtio_gpu_gl_create_rasterizer_object_command* pCommand, uint32_t objectId, struct virtio_gpu_gl_rasterizer_state rasterizerState);
int virtio_gpu_gl_write_create_dsa_object(struct virtio_gpu_gl_create_dsa_object_command* pCommand, uint32_t objectId, struct virtio_gpu_gl_dsa_state dsaState);
int virtio_gpu_gl_write_create_blend_object(struct virtio_gpu_gl_create_blend_object_command* pCommand, uint32_t objectId, uint32_t s0, uint32_t s1, uint32_t blendStateCount, struct virtio_gpu_gl_blend_state* pBlendStateList);
int virtio_gpu_gl_write_set_framebuffer_state_list(struct virtio_gpu_gl_set_framebuffer_state_list_command* pCommand, uint32_t colorBufferCount, uint32_t depthStencilHandle, uint32_t* pColorBufferHandleList);
int virtio_gpu_gl_write_set_viewport_state_list(struct virtio_gpu_gl_set_viewport_state_list_command* pCommand, uint32_t startSlot, uint64_t viewportStateCount, struct virtio_gpu_gl_viewport_state* pViewportStateList);
int virtio_gpu_gl_write_set_scissor_state_list(struct virtio_gpu_gl_set_scissor_state_list_command* pCommand, uint32_t startSlot, uint32_t scissorStateCount, struct virtio_gpu_gl_scissor_state* pScissorStateList);
int virtio_gpu_gl_write_clear(struct virtio_gpu_gl_clear_command* pCommand, uint32_t buffers, struct fvec4_32 color, double depth, uint32_t stencil);
int virtio_gpu_gl_write_transfer_3d(struct virtio_gpu_gl_transfer_3d_command* pCommand, struct virtio_gpu_gl_transfer_3d_command command);
int virtio_gpu_cmd_context_init(uint64_t commandListSize, struct virtio_gpu_cmd_context_desc** ppCmdContextDesc);
int virtio_gpu_cmd_context_deinit(struct virtio_gpu_cmd_context_desc* pCmdContextDesc);
int virtio_gpu_cmd_context_reset(struct virtio_gpu_cmd_context_desc* pCmdContextDesc);
int virtio_gpu_cmd_context_get_desc(uint64_t cmdContextId, struct virtio_gpu_cmd_context_desc** ppCmdContextDesc);
int virtio_gpu_cmd_context_get_current_cmd(struct virtio_gpu_cmd_context_desc* pCmdContextDesc, unsigned char** ppCurrentCommand);
int virtio_gpu_cmd_context_push_cmd(struct virtio_gpu_cmd_context_desc* pCmdContextDesc, uint64_t commandSize);
int virtio_gpu_queue_init(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_queue_deinit(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_notify(uint16_t notifyId);
int virtio_gpu_run_queue(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_response_queue_interrupt(uint8_t interruptVector);
int virtio_gpu_alloc_memory_desc(struct virtio_gpu_queue_info* pQueueInfo, uint64_t physicalAddress, uint32_t size, uint16_t flags, struct virtio_gpu_memory_desc_info* pMemoryDescInfo);
int virtio_gpu_queue_alloc_cmd(struct virtio_gpu_queue_info* pQueueInfo, struct virtio_gpu_alloc_cmd_info cmdAllocInfo, struct virtio_gpu_command_desc* pCommandDesc);
int virtio_gpu_queue_polling_enable(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_queue_polling_disable(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_queue_yield_until_response(struct virtio_gpu_command_desc* pCommandDesc);
int virtio_gpu_subsystem_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pColor);
int virtio_gpu_subsystem_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 color);
int virtio_gpu_subsystem_sync(uint64_t monitorId, struct uvec4 rect);
int virtio_gpu_subsystem_commit(uint64_t monitorId, struct uvec4 rect);
int virtio_gpu_subsystem_push(uint64_t monitorId, struct uvec4 rect);
int virtio_gpu_subsystem_set_scanout(uint64_t monitorId, uint64_t resourceId);
int virtio_gpu_subsystem_cmd_context_init(uint64_t gpuId, uint64_t cmdContextId, uint64_t commandListSize);
int virtio_gpu_subsystem_cmd_context_deinit(uint64_t gpuId, uint64_t cmdContextId);
int virtio_gpu_subsystem_cmd_context_reset(uint64_t gpuId, uint64_t cmdContextId);
int virtio_gpu_subsystem_push_set_framebuffer_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_framebuffer_state_list_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_set_viewport_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_viewport_state_list_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_set_scissor_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_scissor_state_list_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_set_vertex_buffer_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_vertex_buffer_list_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_draw_vbo_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_draw_vbo_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_clear_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_clear_cmd_info* pCommandInfo);
int virtio_gpu_subsystem_push_cmd(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_info_header* pCommandInfo);
int virtio_gpu_subsystem_cmd_context_submit(uint64_t gpuId, uint64_t contextId, uint64_t cmdContextId);
int virtio_gpu_subsystem_rasterizer_state_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_rasterizer_state_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_dsa_state_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_dsa_state_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_blend_state_list_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_blend_state_list_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_shader_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_vertex_element_list_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_vertex_element_list_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_surface_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_surface_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader);
int virtio_gpu_subsystem_object_create(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_object_info* pCreateObjectInfo);
int virtio_gpu_subsystem_object_delete(uint64_t gpuId, uint64_t objectId);
int virtio_gpu_subsystem_object_bind(uint64_t gpuId, uint64_t objectId);
int virtio_gpu_subsystem_context_create(uint64_t gpuId, uint64_t contextId);
int virtio_gpu_subsystem_context_delete(uint64_t gpuId, uint64_t contextId);
int virtio_gpu_subsystem_context_attach_resource(uint64_t gpuId, uint64_t contextId, uint64_t resourceId);
int virtio_gpu_subsystem_resource_create(uint64_t gpuId, uint64_t resourceId, struct gpu_create_resource_info createResourceInfo);
int virtio_gpu_subsystem_resource_delete(uint64_t gpuId, uint64_t resourceId);
int virtio_gpu_subsystem_resource_attach_backing(uint64_t gpuId, uint64_t resourceId, unsigned char* pBuffer, uint64_t bufferSize);
int virtio_gpu_subsystem_resource_create_blob(uint64_t gpuId, uint64_t resourceBlobId, struct gpu_create_resource_blob_info createResourceBlobInfo);
int virtio_gpu_subsystem_resource_flush(uint64_t gpuId, uint64_t resourceId, struct gpu_rect rect);
int virtio_gpu_subsystem_transfer_to_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_to_device_info transferInfo);
int virtio_gpu_subsystem_transfer_from_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_from_device_info transferInfo);
int virtio_gpu_subsystem_panic_entry(uint64_t driverId);
int virtio_gpu_response_queue_isr(void);
#endif
