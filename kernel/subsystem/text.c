#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "subsystem/gpu.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "math/vector.h"
#include "math/trig.h"
#include "align.h"
#include "drivers/timer.h"
#include "drivers/filesystem.h"
#include "drivers/serial.h"
#include "subsystem/text.h"
unsigned int char_position = 0;
struct uvec4_8 text_fg = {255,255,255,255};
struct uvec4_8 text_bg = {16,16,16,255};
static struct text_subsystem_info textSubsystemInfo = {0};
int text_subsystem_soft_init(void){
	pbootargs->graphicsInfo.fontDataSize = sizeof(mainfont_data);
	pbootargs->graphicsInfo.fontData = (unsigned char*)mainfont_data;
	pbootargs->graphicsInfo.font_initialized = 1;
	return 0;
}
int text_subsystem_init(void){
	if (gpu_monitor_exists(1)!=0){
		printf("no GPU host controller monitor for text subsystem\r\n");
		return 0;	
	}
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(1, &pMonitorDesc)!=0){
		printf("failed to get GPU host controller monitor descriptor\r\n");
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		return -1;
	}
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(pMonitorDesc->gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		return -1;
	}
	struct subsystem_desc* pFontDriverSubsystemDesc = (struct subsystem_desc*)0x00;
	if (subsystem_init(&pFontDriverSubsystemDesc, TEXT_SUBSYSTEM_MAX_FONT_DRIVER_COUNT)!=0){
		printf("failed to initialize font driver subsystem\r\n");
		return -1;
	}
	struct subsystem_desc* pFontSubsystemDesc = (struct subsystem_desc*)0x00;
	if (subsystem_init(&pFontSubsystemDesc, TEXT_SUBSYSTEM_MAX_FONT_COUNT)!=0){
		printf("failed to initialize font subsystem\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		return -1;
	}
	uint64_t contextId = pMonitorDesc->monitorInfo.framebufferContextId;
	uint64_t surfaceObjectId = 0x00;
	struct gpu_create_surface_object_info createSurfaceObjectInfo = {0};
	memset((void*)&createSurfaceObjectInfo, 0, sizeof(struct gpu_create_surface_object_info));
	createSurfaceObjectInfo.header.objectType = GPU_OBJECT_TYPE_SURFACE;
	createSurfaceObjectInfo.resourceId = pMonitorDesc->monitorInfo.framebufferResourceId;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createSurfaceObjectInfo, &surfaceObjectId)!=0){
		printf("failed to create GPU host controller surface object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t commandContextId = 0x00;
	uint64_t commandBufferSize = PAGE_SIZE*0x04;
	if (gpu_cmd_context_init(pMonitorDesc->gpuId, commandBufferSize, &commandContextId)!=0){
		printf("failed to create GPU host controller command context\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	gpu_cmd_context_reset(pMonitorDesc->gpuId, commandContextId);
	struct gpu_set_framebuffer_state_list_cmd_info setFramebufferStateListCmdInfo = {0};
	memset((void*)&setFramebufferStateListCmdInfo, 0, sizeof(struct gpu_set_framebuffer_state_list_cmd_info));
	uint32_t colorBufferHandle = (uint32_t)surfaceObjectId;
	setFramebufferStateListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_FRAMEBUFFER_STATE_LIST;
	setFramebufferStateListCmdInfo.colorBufferCount = 0x01;
	setFramebufferStateListCmdInfo.pColorBufferHandleList = (uint32_t*)&colorBufferHandle;
	struct gpu_set_viewport_state_list_cmd_info setViewportStateListCmdInfo = {0};
	memset((void*)&setViewportStateListCmdInfo, 0, sizeof(struct gpu_set_viewport_state_list_cmd_info));
	struct gpu_viewport_state viewportStateList[1] = {0};
	memset((void*)viewportStateList, 0, sizeof(struct gpu_viewport_state));
	viewportStateList[0].translate.x = ((float)pMonitorDesc->monitorInfo.resolution.width)/2.0f;
	viewportStateList[0].translate.y = ((float)pMonitorDesc->monitorInfo.resolution.height)/2.0f;
	viewportStateList[0].translate.z = 0.5f;
	viewportStateList[0].scale.x = ((float)pMonitorDesc->monitorInfo.resolution.width)/2.0f;
	viewportStateList[0].scale.y = ((float)pMonitorDesc->monitorInfo.resolution.height)/2.0f;
	viewportStateList[0].scale.z = 0.5f;
	setViewportStateListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VIEWPORT_STATE_LIST;
	setViewportStateListCmdInfo.viewportStateCount = 0x01;
	setViewportStateListCmdInfo.pViewportStateList = (struct gpu_viewport_state*)viewportStateList;
	struct gpu_set_scissor_state_list_cmd_info setScissorStateListCmdInfo = {0};
	memset((void*)&setScissorStateListCmdInfo, 0, sizeof(struct gpu_set_scissor_state_list_cmd_info));
	struct gpu_scissor_state scissorStateList[1] = {0};
	memset((void*)scissorStateList, 0, sizeof(struct gpu_scissor_state));
	scissorStateList[0].maxX = pMonitorDesc->monitorInfo.resolution.width;
	scissorStateList[0].maxY = pMonitorDesc->monitorInfo.resolution.height;
	setScissorStateListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_SCISSOR_STATE_LIST;
	setScissorStateListCmdInfo.scissorStateCount = 0x01;
	setScissorStateListCmdInfo.pScissorStateList = (struct gpu_scissor_state*)scissorStateList;
	gpu_cmd_context_push_cmd(pMonitorDesc->gpuId, commandContextId, (struct gpu_cmd_info_header*)&setFramebufferStateListCmdInfo);
	gpu_cmd_context_push_cmd(pMonitorDesc->gpuId, commandContextId, (struct gpu_cmd_info_header*)&setViewportStateListCmdInfo);
	gpu_cmd_context_push_cmd(pMonitorDesc->gpuId, commandContextId, (struct gpu_cmd_info_header*)&setScissorStateListCmdInfo);
	if (gpu_cmd_context_submit(pMonitorDesc->gpuId, contextId, commandContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t glyphTextureResourceId = 0x00;
	struct uvec2_32 glyphTextureRect = {0};
	glyphTextureRect.x = 0x20;
	glyphTextureRect.y = 0x20;
	glyphTextureRect.x = 0x40;
	glyphTextureRect.y = 0x40;
	glyphTextureRect.x = 0x200;
	glyphTextureRect.y = 0x200;
	struct gpu_create_resource_info createResourceInfo = {0};
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R16_SNORM;
	createResourceInfo.resourceInfo.normal.width = glyphTextureRect.x;
	createResourceInfo.resourceInfo.normal.height = glyphTextureRect.y;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.arraySize = 0x00;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_TEXTURE_2D;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_SAMPLER_VIEW|GPU_BIND_RENDER_TARGET;
	if (gpu_resource_create(pMonitorDesc->gpuId, createResourceInfo, &glyphTextureResourceId)!=0){
		printf("failed to create GPU host controller glyph texture buffer resource\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	if (gpu_context_attach_resource(pMonitorDesc->gpuId, contextId, glyphTextureResourceId)!=0){
		printf("failed to attach GPU host controller resource to GPU host controller context\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t glyphVertexBufferResourceId = 0x00;
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32G32_FLOAT;
	createResourceInfo.resourceInfo.normal.width = sizeof(struct text_subsystem_glyph_vertex)*0x06;
	createResourceInfo.resourceInfo.normal.height = 0x01;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.arraySize = 0x01;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_VERTEX_BUFFER;
	if (gpu_resource_create(pMonitorDesc->gpuId, createResourceInfo, &glyphVertexBufferResourceId)!=0){
		printf("failed to create GPU host controller vertex buffer resource\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	if (gpu_context_attach_resource(pMonitorDesc->gpuId, contextId, glyphVertexBufferResourceId)!=0){
		printf("failed to attach GPU host controller resource to GPU host controller context\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t rasterizerStateObjectId = 0x00;
	struct gpu_create_rasterizer_state_object_info createRasterizerStateObjectInfo = {0};
	memset((void*)&createRasterizerStateObjectInfo, 0, sizeof(struct gpu_create_rasterizer_state_object_info));
	struct gpu_rasterizer_state rasterizerState = {0};
	memset((void*)&rasterizerState, 0, sizeof(struct gpu_rasterizer_state));
	rasterizerState.half_pixel_center = 0x01;
	rasterizerState.fill_mode_front = GPU_POLYGON_MODE_FILL;
	rasterizerState.fill_mode_back = GPU_POLYGON_MODE_FILL;
	rasterizerState.point_size = 4.0f;
	rasterizerState.line_width = 4.0f;
	createRasterizerStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_RASTERIZER_STATE;
	createRasterizerStateObjectInfo.surfaceObjectId = surfaceObjectId;
	createRasterizerStateObjectInfo.pRasterizerState = &rasterizerState;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createRasterizerStateObjectInfo, &rasterizerStateObjectId)!=0){
		printf("failed to create GPU host controller rasterizer state object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t dsaStateObjectId = 0x00;
	struct gpu_create_dsa_state_object_info createDsaStateObjectInfo = {0};
	memset((void*)&createDsaStateObjectInfo, 0, sizeof(struct gpu_create_dsa_state_object_info));
	struct gpu_dsa_state dsaState = {0};
	memset((void*)&dsaState, 0, sizeof(struct gpu_dsa_state));
	dsaState.alpha_enable = 0x01;
	dsaState.alpha_func = GPU_FUNC_ALWAYS;
	createDsaStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_DSA_STATE;
	createDsaStateObjectInfo.surfaceObjectId = surfaceObjectId;
	createDsaStateObjectInfo.pDsaState = &dsaState;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createDsaStateObjectInfo, &dsaStateObjectId)!=0){
		printf("failed to create GPU host controller DSA state object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t blendStateListObjectId = 0x00;
	struct gpu_create_blend_state_list_object_info createBlendStateListObjectInfo = {0};
	memset((void*)&createBlendStateListObjectInfo, 0, sizeof(struct gpu_create_blend_state_list_object_info));
	struct gpu_blend_state blendStateList[1] = {0};
	memset((void*)blendStateList, 0, sizeof(struct gpu_blend_state));
	blendStateList[0].blend_enable = 0x01;
	blendStateList[0].rgb_func = GPU_BLEND_ADD;
	blendStateList[0].rgb_src_factor = GPU_BLENDFACTOR_SRC_ALPHA;
	blendStateList[0].rgb_dest_factor = GPU_BLENDFACTOR_INV_SRC_ALPHA;
	blendStateList[0].alpha_func = GPU_BLEND_ADD;
	blendStateList[0].alpha_src_factor = GPU_BLENDFACTOR_ONE;	
	blendStateList[0].alpha_dest_factor = GPU_BLENDFACTOR_ZERO;
	blendStateList[0].color_mask = GPU_COLOR_MASK_RGBA;
	createBlendStateListObjectInfo.header.objectType = GPU_OBJECT_TYPE_BLEND_STATE_LIST;
	createBlendStateListObjectInfo.surfaceObjectId = surfaceObjectId;
	createBlendStateListObjectInfo.pBlendStateList = (struct gpu_blend_state*)blendStateList;
	createBlendStateListObjectInfo.blendStateCount = 0x01;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createBlendStateListObjectInfo, &blendStateListObjectId)!=0){
		printf("failed to create GPU host controller blend state list object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t vertexElementListObjectId = 0x00;
	struct gpu_create_vertex_element_list_object_info createVertexElementListObjectInfo = {0};
	memset((void*)&createVertexElementListObjectInfo, 0, sizeof(struct gpu_create_vertex_element_list_object_info));
	struct gpu_vertex_element vertexElementList[3] = {0};
	memset((void*)vertexElementList, 0, sizeof(struct gpu_vertex_element)*0x03);
	vertexElementList[0].src_offset = 0x00;
	vertexElementList[0].vertex_buffer_index = 0x00;
	vertexElementList[0].src_format = GPU_FORMAT_R32G32_FLOAT;
	vertexElementList[1].src_offset = sizeof(struct fvec2_32);
	vertexElementList[1].vertex_buffer_index = 0x00;
	vertexElementList[1].src_format = GPU_FORMAT_R32G32_FLOAT;
	vertexElementList[2].src_offset = vertexElementList[1].src_offset+sizeof(struct fvec2_32);
	vertexElementList[2].vertex_buffer_index = 0x00;
	vertexElementList[2].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
	createVertexElementListObjectInfo.header.objectType = GPU_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
	createVertexElementListObjectInfo.surfaceObjectId = surfaceObjectId;
	createVertexElementListObjectInfo.pVertexElementList = (struct gpu_vertex_element*)vertexElementList;
	createVertexElementListObjectInfo.vertexElementCount = 0x03;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createVertexElementListObjectInfo, &vertexElementListObjectId)!=0){
		printf("failed to create GPU host controller vertex element list object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	static const unsigned char vertexShaderCode[]="VERT\n"
		"DCL OUT[0], POSITION\n"
		"DCL OUT[1], TEXCOORD\n"
		"DCL OUT[2], COLOR\n"
		"DCL CONST[0..1]\n"
		"DCL TEMP[0]\n"
		"DCL IN[0]\n"
		"DCL IN[1]\n"
		"DCL IN[2]\n"
		"MOV TEMP[0], IN[0]\n"
		"MUL TEMP[0].xy, TEMP[0], CONST[1]\n"
		"ADD TEMP[0].xy, TEMP[0], CONST[0]\n"
		"MOV OUT[0], TEMP[0]\n"
		"MOV OUT[1], IN[1]\n"
		"MOV OUT[2], IN[2]\n"
		"END\n";
	static const unsigned char fragmentShaderCode[]="FRAG\n"
		"DCL OUT[0], COLOR\n"
		"DCL IN[0], COLOR, PERSPECTIVE\n"
		"DCL IN[1], TEXCOORD, PERSPECTIVE\n"
		"DCL SAMP[0]\n"
		"DCL SVIEW[0], 2D, FLOAT\n"
		"DCL TEMP[0]\n"
		"TEX TEMP[0], IN[1], SAMP[0], 2D\n"
		"DCL TEMP[1]\n"
		"MOV TEMP[1], IN[0]\n"
		"DCL TEMP[2]\n"
		"IMM[0] FLT32 {0.0, 0.0, 0.0, 0.0}\n"
		"IMM[1] FLT32 {1.0, 1.0, 1.0, 0.0}\n"
		"SLT TEMP[2].x, IMM[0].xxxx, TEMP[0].xxxx\n"
		"IF TEMP[2].xxxx\n"
		"  MOV TEMP[1], IMM[1]\n"
		"ENDIF\n"
		"MOV OUT[0], TEMP[1]\n"
		"END\n";
	uint64_t vertexShaderObjectId = 0x00;
	uint64_t fragmentShaderObjectId = 0x00;
	struct gpu_create_shader_object_info createShaderObjectInfo = {0};
	memset((void*)&createShaderObjectInfo, 0, sizeof(struct gpu_create_shader_object_info));
	createShaderObjectInfo.header.objectType = GPU_OBJECT_TYPE_SHADER;
	createShaderObjectInfo.surfaceObjectId = surfaceObjectId;
	createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
	createShaderObjectInfo.pShaderCode = (unsigned char*)vertexShaderCode;
	createShaderObjectInfo.shaderCodeSize = sizeof(vertexShaderCode);
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &vertexShaderObjectId)!=0){
		printf("failed to create GPU host controller vertex shader object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
	createShaderObjectInfo.pShaderCode = (unsigned char*)fragmentShaderCode;
	createShaderObjectInfo.shaderCodeSize = sizeof(fragmentShaderCode);
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &fragmentShaderObjectId)!=0){
		printf("failed to create GPU host controller fragment shader object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t samplerStateObjectId = 0x00;
	struct gpu_create_sampler_state_object_info createSamplerStateObjectInfo = {0};
	memset((void*)&createSamplerStateObjectInfo, 0, sizeof(struct gpu_create_sampler_state_object_info));
	struct gpu_sampler_state samplerState = {0};
	memset((void*)&samplerState, 0, sizeof(struct gpu_sampler_state));
	samplerState.wrap_s = GPU_TEXTURE_WRAP_CLAMP_TO_EDGE;
	samplerState.wrap_t = GPU_TEXTURE_WRAP_CLAMP_TO_EDGE;
	samplerState.wrap_r = GPU_TEXTURE_WRAP_CLAMP_TO_EDGE;
	samplerState.magImgFilter = GPU_TEXTURE_FILTER_NEAREST;
	samplerState.minImgFilter = GPU_TEXTURE_FILTER_NEAREST;
	samplerState.minMipFilter = GPU_TEXTURE_FILTER_NEAREST;
	samplerState.minDetailLevel = 16.0f;
	samplerState.maxDetailLevel = 31.0f;
	createSamplerStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_SAMPLER_STATE;
	createSamplerStateObjectInfo.samplerState = samplerState;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createSamplerStateObjectInfo, &samplerStateObjectId)!=0){
		printf("failed to create GPU host controller sampler state object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t samplerViewObjectId = 0x00;
	struct gpu_create_sampler_view_object_info createSamplerViewObjectInfo = {0};
	memset((void*)&createSamplerViewObjectInfo, 0, sizeof(struct gpu_create_sampler_view_object_info));
	createSamplerViewObjectInfo.header.objectType = GPU_OBJECT_TYPE_SAMPLER_VIEW;
	createSamplerViewObjectInfo.resourceId = glyphTextureResourceId;
	createSamplerViewObjectInfo.format = GPU_FORMAT_R16_SNORM;
	createSamplerViewObjectInfo.firstLevel = 0x00;
	createSamplerViewObjectInfo.lastLevel = 0x00;
	createSamplerViewObjectInfo.swizzle.r = 0x00;
	createSamplerViewObjectInfo.swizzle.g = 0x00;
	createSamplerViewObjectInfo.swizzle.b = 0x00;
	createSamplerViewObjectInfo.swizzle.a = 0x00;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createSamplerViewObjectInfo, &samplerViewObjectId)!=0){
		printf("failed to create GPU host controller sampler view object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	struct text_subsystem_glyph_vertex* pGlyphVertexBuffer = (struct text_subsystem_glyph_vertex*)0x00;
	uint64_t glyphVertexBufferSize = sizeof(struct text_subsystem_glyph_vertex)*0x06;
	if (virtualAlloc((uint64_t*)&pGlyphVertexBuffer, glyphVertexBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for GPU host controller vertex buffer\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	memset((void*)pGlyphVertexBuffer, 0, glyphVertexBufferSize);
	pGlyphVertexBuffer[0].position.x = -1.0f;
	pGlyphVertexBuffer[0].position.y = -1.0f;
	pGlyphVertexBuffer[0].textureCoord.x = 0.0f;	
	pGlyphVertexBuffer[0].textureCoord.y = 0.0f;
	pGlyphVertexBuffer[1].position.x = -1.0f;
	pGlyphVertexBuffer[1].position.y = 1.0f;
	pGlyphVertexBuffer[1].textureCoord.x = 0.0f;
	pGlyphVertexBuffer[1].textureCoord.y = 1.0f;
	pGlyphVertexBuffer[2].position.x = 1.0f;
	pGlyphVertexBuffer[2].position.y = -1.0f;
	pGlyphVertexBuffer[2].textureCoord.x = 1.0f;
	pGlyphVertexBuffer[2].textureCoord.y = 0.0f;
	pGlyphVertexBuffer[3].position.x = -1.0f;
	pGlyphVertexBuffer[3].position.y = 1.0f;
	pGlyphVertexBuffer[3].textureCoord.x = 0.0f;
	pGlyphVertexBuffer[3].textureCoord.y = 1.0f;
	pGlyphVertexBuffer[4].position.x = 1.0f;
	pGlyphVertexBuffer[4].position.y = 1.0f;
	pGlyphVertexBuffer[4].textureCoord.x = 1.0f;
	pGlyphVertexBuffer[4].textureCoord.y = 1.0f;
	pGlyphVertexBuffer[5].position.x = 1.0f;
	pGlyphVertexBuffer[5].position.y = -1.0f;
	pGlyphVertexBuffer[5].textureCoord.x = 1.0f;
	pGlyphVertexBuffer[5].textureCoord.y = 0.0f;
	for (uint64_t i = 0;i<glyphVertexBufferSize/sizeof(struct text_subsystem_glyph_vertex);i++){
		struct fvec4_32 vertexColor = {0};
		vertexColor.x = 1.0f;
		vertexColor.y = 0.0f;
		vertexColor.z = 1.0f;
		vertexColor.w = 1.0f;
		pGlyphVertexBuffer[i].color = vertexColor;
	}
	if (gpu_resource_attach_backing(pMonitorDesc->gpuId, glyphVertexBufferResourceId, (unsigned char*)pGlyphVertexBuffer, PAGE_SIZE)!=0){
		printf("failed to attach GPU host controller vertex buffer physical pages to GPU host controller vertex buffer resource\r\n");
		virtualFree((uint64_t)pGlyphVertexBuffer, glyphVertexBufferSize);
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	struct gpu_transfer_to_device_info transferToDeviceInfo = {0};
	memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
	transferToDeviceInfo.boxRect.width = glyphVertexBufferSize;
	transferToDeviceInfo.boxRect.height = 0x01;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(pMonitorDesc->gpuId, glyphVertexBufferResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller vertex buffer resource physical pages to GPU host controller\r\n");
		virtualFree((uint64_t)pGlyphVertexBuffer, glyphVertexBufferSize);
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;	
	}
	textSubsystemInfo.pMonitorDesc = pMonitorDesc;
	textSubsystemInfo.pDriverDesc = pDriverDesc;
	textSubsystemInfo.pGpuDesc = pGpuDesc;
	textSubsystemInfo.accelerationInfo.contextId = contextId;
	textSubsystemInfo.accelerationInfo.surfaceObjectId = surfaceObjectId;
	textSubsystemInfo.accelerationInfo.glyphTextureResourceId = glyphTextureResourceId;
	textSubsystemInfo.accelerationInfo.glyphTextureRect = glyphTextureRect;
	textSubsystemInfo.accelerationInfo.glyphVertexBufferResourceId = glyphVertexBufferResourceId;
	textSubsystemInfo.accelerationInfo.pGlyphVertexBuffer = pGlyphVertexBuffer;
	textSubsystemInfo.accelerationInfo.glyphVertexBufferSize = glyphVertexBufferSize;
	textSubsystemInfo.accelerationInfo.rasterizerStateObjectId = rasterizerStateObjectId;
	textSubsystemInfo.accelerationInfo.dsaStateObjectId = dsaStateObjectId;
	textSubsystemInfo.accelerationInfo.blendStateListObjectId = blendStateListObjectId;
	textSubsystemInfo.accelerationInfo.vertexElementListObjectId = vertexElementListObjectId;
	textSubsystemInfo.accelerationInfo.vertexShaderObjectId = vertexShaderObjectId;
	textSubsystemInfo.accelerationInfo.fragmentShaderObjectId = fragmentShaderObjectId;
	textSubsystemInfo.accelerationInfo.glyphSamplerStateObjectId = samplerStateObjectId;
	textSubsystemInfo.accelerationInfo.glyphSamplerViewObjectId = samplerViewObjectId;
	textSubsystemInfo.accelerationInfo.commandContextId = commandContextId;
	textSubsystemInfo.accelerationInfo.commandBufferSize = commandBufferSize;
	textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc = pFontDriverSubsystemDesc;
	textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc = pFontSubsystemDesc;
	return 0;
}
int text_subsystem_deinit(void){
	if (!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	if (subsystem_deinit(textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)!=0){
		printf("failed to deinitialize font driver subsystem\r\n");
		return -1;
	}
	if (subsystem_deinit(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc)!=0){
		printf("failed to deinitialize font subsystem\r\n");
		return -1;
	}
	if (gpu_cmd_context_deinit(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId)!=0){
		printf("failed to deinitialize GPU host controller command context\r\n");
		return -1;
	}
	return 0;
}
KAPI int text_subsystem_font_driver_register(struct text_subsystem_font_driver_info* pSubsystemFontDriverInfo){
	if (!pSubsystemFontDriverInfo||!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct text_subsystem_font_driver_desc* pFontDriverDesc = (struct text_subsystem_font_driver_desc*)kmalloc(sizeof(struct text_subsystem_font_driver_desc));
	uint64_t fontDriverId = 0x00;
	if (!pFontDriverDesc){
		printf("failed to allocate text subsystem font driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_alloc_entry(textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc, (unsigned char*)pFontDriverDesc, &fontDriverId)!=0){
		printf("failed to allocate text subsystem font driver descriptor entry\r\n");
		kfree((void*)pFontDriverDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pFontDriverDesc, 0, sizeof(struct text_subsystem_font_driver_desc));
	pSubsystemFontDriverInfo->fontDriverId = fontDriverId;
	pSubsystemFontDriverInfo->maxTextureBufferRect = textSubsystemInfo.accelerationInfo.glyphTextureRect;
	pFontDriverDesc->driverInfo = *pSubsystemFontDriverInfo;
	if (!textSubsystemInfo.fontDriverSubsystemInfo.pFirstFontDriverDesc)
		textSubsystemInfo.fontDriverSubsystemInfo.pFirstFontDriverDesc = pFontDriverDesc;
	if (textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc){
		textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc->pFlink = pFontDriverDesc;
		pFontDriverDesc->pBlink = textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc;
	}
	textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc = pFontDriverDesc;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int text_subsystem_font_driver_unregister(uint64_t fontDriverId){
	if (!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct text_subsystem_font_driver_desc* pFontDriverDesc = (struct text_subsystem_font_driver_desc*)0x00;
	if (subsystem_get_entry(textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc, fontDriverId, (uint64_t*)&pFontDriverDesc)!=0){
		printf("failed to get text subsystem font driver descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pFontDriverDesc->driverInfo.vtable.fontDriverDeinit()!=0){
		printf("failed to deinitialize text subsystem font driver\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pFontDriverDesc==textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc)
		textSubsystemInfo.fontDriverSubsystemInfo.pLastFontDriverDesc = pFontDriverDesc->pBlink;
	if (pFontDriverDesc==textSubsystemInfo.fontDriverSubsystemInfo.pFirstFontDriverDesc)
		textSubsystemInfo.fontDriverSubsystemInfo.pFirstFontDriverDesc = pFontDriverDesc->pFlink;
	if (pFontDriverDesc->pFlink)	
		pFontDriverDesc->pFlink->pBlink = pFontDriverDesc->pBlink;
	if (pFontDriverDesc->pBlink)
		pFontDriverDesc->pBlink->pFlink = pFontDriverDesc->pFlink;
	kfree((void*)pFontDriverDesc);
	mutex_unlock(&mutex);
	return 0;
}
KAPI int text_subsystem_font_load(unsigned char* pFontBuffer, uint64_t fontBufferSize, uint64_t* pFontId){
	if (!pFontBuffer||!fontBufferSize||!pFontId||!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct text_subsystem_font_desc* pFontDesc = (struct text_subsystem_font_desc*)kmalloc(sizeof(struct text_subsystem_font_desc));
	uint64_t fontId = 0x00;
	if (!pFontDesc){
		printf("failed to allocate text subsystem font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_alloc_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, (unsigned char*)pFontDesc, &fontId)!=0){
		printf("failed to allocate text subsystem entry for font descriptor\r\n");
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pFontDesc, 0, sizeof(struct text_subsystem_font_desc));
	pFontDesc->fontId = fontId;
	pFontDesc->pFontBuffer = pFontBuffer;
	pFontDesc->fontBufferSize = fontBufferSize;
	struct text_subsystem_font_driver_desc* pFontDriverDesc = textSubsystemInfo.fontDriverSubsystemInfo.pFirstFontDriverDesc;
	uint64_t fontLoaded = 0x00;
	while (pFontDriverDesc){
		if (pFontDriverDesc->driverInfo.vtable.fontVerify(pFontBuffer, fontBufferSize)!=0){
			pFontDriverDesc = pFontDriverDesc->pFlink;
			continue;
		}
		struct text_subsystem_font_name_desc** ppFontNameDescList = (struct text_subsystem_font_name_desc**)0x00;
		uint64_t fontNameDescListSize = sizeof(struct text_subsystem_font_name_desc*)*TEXT_SUBSYSTEM_MAX_FONT_NAME_COUNT;
		if (virtualAlloc((uint64_t*)&ppFontNameDescList, fontNameDescListSize, PTE_RW|PTE_NX, 0x00, PAGE_TYPE_NORMAL)!=0){
			printf("failed to allocate physical pages for font name descriptor list\r\n");
			subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
			kfree((void*)pFontDesc);
			mutex_unlock(&mutex);
			return -1;
		}
		memset((void*)ppFontNameDescList, 0, fontNameDescListSize);
		pFontDesc->ppFontNameDescList = ppFontNameDescList;
		pFontDesc->fontNameDescListSize = fontNameDescListSize;
		if (pFontDriverDesc->driverInfo.vtable.fontLoad(pFontBuffer, fontBufferSize, pFontDesc)!=0){
			pFontDriverDesc = pFontDriverDesc->pFlink;
			continue;
		}
		fontLoaded = 0x01;
		break;
	}
	if (!fontLoaded){
		printf("failed to find GPU host controller font driver to load font\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (!textSubsystemInfo.fontSubsystemInfo.pFirstFontDesc)
		textSubsystemInfo.fontSubsystemInfo.pFirstFontDesc = pFontDesc;
	if (textSubsystemInfo.fontSubsystemInfo.pLastFontDesc){
		textSubsystemInfo.fontSubsystemInfo.pLastFontDesc->pFlink = pFontDesc;
		pFontDesc->pBlink = textSubsystemInfo.fontSubsystemInfo.pLastFontDesc;
	}
	textSubsystemInfo.fontSubsystemInfo.pLastFontDesc = pFontDesc;
	uint64_t glyphId = 0x00;
	if (pFontDriverDesc->driverInfo.vtable.fontGlyphGetId(pFontDesc, (uint64_t)'O', &glyphId)!=0){
		printf("failed to get glyph ID via font driver\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	printf("glyph ID: %d\r\n", glyphId);
	int16_t* pGlyphTextureBuffer = (int16_t*)0x00;
	struct uvec2_32 glyphTextureBufferRect = {0};
	glyphTextureBufferRect.x = textSubsystemInfo.accelerationInfo.glyphTextureRect.x;
	glyphTextureBufferRect.y = textSubsystemInfo.accelerationInfo.glyphTextureRect.y;
	uint64_t glyphTextureBufferSize = (glyphTextureBufferRect.x*glyphTextureBufferRect.y)*sizeof(int16_t);
	if (virtualAlloc((uint64_t*)&pGlyphTextureBuffer, glyphTextureBufferSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for glyph texture buffer\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct fvec4_32 backgroundColor = {0};
	backgroundColor.x = 1.0f;
	backgroundColor.y = 1.0f;
	backgroundColor.z = 1.0f;
	backgroundColor.w = 1.0f;
	for (uint64_t i = 0;i<glyphTextureBufferSize/sizeof(int16_t);i++){
		*(((int16_t*)pGlyphTextureBuffer)+i) = 0x7FFF;
	}
	if (pFontDriverDesc->driverInfo.vtable.fontGlyphTesselate(pFontDesc, glyphId, pGlyphTextureBuffer, glyphTextureBufferRect)!=0){
		printf("failed to tesselate font glyph via font driver\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.rasterizerStateObjectId)!=0){
		printf("failed to bind GPU host controller rasterizer state object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.dsaStateObjectId)!=0){
		printf("failed to bind GPU Host controller DSA state object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.blendStateListObjectId)!=0){
		printf("failed to bind GPU host controller blend state list object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexElementListObjectId)!=0){
		printf("failed to bind GPU host controller vertex element list object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexShaderObjectId)!=0){
		printf("failed to bind GPU host controller vertex shader object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.fragmentShaderObjectId)!=0){
		printf("failed to bind GPU host controller fragment shader object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_resource_attach_backing(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.glyphTextureResourceId, (unsigned char*)pGlyphTextureBuffer, glyphTextureBufferSize)!=0){
		printf("failed to attach GPU host controller texture resource physical pages to GPU host controller resource\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_transfer_to_device_info transferToDeviceInfo = {0};
	memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
	transferToDeviceInfo.boxRect.width = glyphTextureBufferRect.x;
	transferToDeviceInfo.boxRect.height = glyphTextureBufferRect.y;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.glyphTextureResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller texture buffer resource physical pages to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct fvec4_32 constantBuffer[2] = {0};
	struct fvec4_32* pTranslationVector = (struct fvec4_32*)constantBuffer;
	pTranslationVector->x = 0.5f;
	pTranslationVector->y = 0.5f;
	pTranslationVector->z = 0.0f;
	pTranslationVector->w = 1.0f;
	struct fvec4_32* pScaleVector = pTranslationVector+0x01;
	pScaleVector->x = 0.25f;
	pScaleVector->y = 0.5f;
	pScaleVector->z = 0.0f;
	pScaleVector->w = 1.0f;
	gpu_cmd_context_reset(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId);
	struct gpu_set_vertex_buffer_list_cmd_info setVertexBufferListCmdInfo = {0};
	memset((void*)&setVertexBufferListCmdInfo, 0, sizeof(struct gpu_set_vertex_buffer_list_cmd_info));
	struct gpu_vertex_buffer vertexBufferList[1] = {0};
	memset((void*)vertexBufferList, 0, sizeof(struct gpu_vertex_buffer));
	vertexBufferList[0].stride = sizeof(struct text_subsystem_glyph_vertex);
	vertexBufferList[0].resource_id = textSubsystemInfo.accelerationInfo.glyphVertexBufferResourceId;
	setVertexBufferListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VERTEX_BUFFER_LIST;
	setVertexBufferListCmdInfo.vertexBufferCount = 0x01;
	setVertexBufferListCmdInfo.pVertexBufferList = (struct gpu_vertex_buffer*)vertexBufferList;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setVertexBufferListCmdInfo);
	struct gpu_bind_sampler_state_list_cmd_info bindSamplerStateListCmdInfo = {0};
	memset((void*)&bindSamplerStateListCmdInfo, 0, sizeof(struct gpu_bind_sampler_state_list_cmd_info));
	uint32_t samplerStateList[1] = {0};
	samplerStateList[0] = (uint32_t)textSubsystemInfo.accelerationInfo.glyphSamplerStateObjectId;
	bindSamplerStateListCmdInfo.header.commandType = GPU_CMD_TYPE_BIND_SAMPLER_STATE_LIST;
	bindSamplerStateListCmdInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
	bindSamplerStateListCmdInfo.startSlot = 0x00;
	bindSamplerStateListCmdInfo.samplerStateCount = 0x01;
	bindSamplerStateListCmdInfo.pSamplerStateList = (uint32_t*)samplerStateList;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&bindSamplerStateListCmdInfo);
	struct gpu_set_sampler_view_list_cmd_info setSamplerViewListCmdInfo = {0};
	memset((void*)&setSamplerViewListCmdInfo, 0, sizeof(struct gpu_set_sampler_view_list_cmd_info));
	uint32_t samplerViewList[1] = {0};
	samplerViewList[0] = (uint32_t)textSubsystemInfo.accelerationInfo.glyphSamplerViewObjectId;
	setSamplerViewListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_SAMPLER_VIEW_LIST;
	setSamplerViewListCmdInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
	setSamplerViewListCmdInfo.startSlot = 0x00;
	setSamplerViewListCmdInfo.samplerViewCount = 0x01;
	setSamplerViewListCmdInfo.pSamplerViewList = (uint32_t*)samplerViewList;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setSamplerViewListCmdInfo);
	struct gpu_set_constant_buffer_cmd_info setConstantBufferCmdInfo = {0};
	memset((void*)&setConstantBufferCmdInfo, 0, sizeof(struct gpu_set_constant_buffer_cmd_info));
	setConstantBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_CONSTANT_BUFFER;
	setConstantBufferCmdInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
	setConstantBufferCmdInfo.index = 0x00;
	setConstantBufferCmdInfo.pBuffer = (unsigned char*)constantBuffer;
	setConstantBufferCmdInfo.bufferSize = sizeof(struct fvec4_32)*0x02;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setConstantBufferCmdInfo);
	struct gpu_draw_vbo_cmd_info drawVboCmdInfo = {0};
	memset((void*)&drawVboCmdInfo, 0, sizeof(struct gpu_draw_vbo_cmd_info));
	struct gpu_draw_vbo_info drawVboInfo = {0};
	memset((void*)&drawVboInfo, 0, sizeof(struct gpu_draw_vbo_info));
	drawVboInfo.start = 0x00;
	drawVboInfo.count = textSubsystemInfo.accelerationInfo.glyphVertexBufferSize/sizeof(struct text_subsystem_glyph_vertex);
	drawVboInfo.mode = GPU_PRIMITIVE_TRIANGLES;
	drawVboInfo.instance_count = 0x01;
	drawVboInfo.max_index = drawVboInfo.count-1;
	drawVboCmdInfo.header.commandType = GPU_CMD_TYPE_DRAW_VBO;
	drawVboCmdInfo.pDrawVboInfo = &drawVboInfo;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&drawVboCmdInfo);
	if (gpu_cmd_context_submit(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.contextId, textSubsystemInfo.accelerationInfo.commandContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFree((uint64_t)pGlyphTextureBuffer, glyphTextureBufferSize);
	struct gpu_transfer_from_device_info transferFromDeviceInfo = {0};
	memset((void*)&transferFromDeviceInfo, 0, sizeof(struct gpu_transfer_from_device_info));
	transferFromDeviceInfo.boxRect.x = 0x00;
	transferFromDeviceInfo.boxRect.y = 0x00;
	transferFromDeviceInfo.boxRect.width = textSubsystemInfo.pMonitorDesc->monitorInfo.resolution.width;
	transferFromDeviceInfo.boxRect.height = textSubsystemInfo.pMonitorDesc->monitorInfo.resolution.height;
	transferFromDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_from_device(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.pMonitorDesc->monitorInfo.framebufferResourceId, transferFromDeviceInfo)!=0){
		printf("failed to transfer GPU host controller framebuffer data to GPU host controller framebuffer physical pages\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_rect flushRect = {0};
	flushRect.x = 0x00;
	flushRect.y = 0x00;
	flushRect.width = textSubsystemInfo.pMonitorDesc->monitorInfo.resolution.width;
	flushRect.height = textSubsystemInfo.pMonitorDesc->monitorInfo.resolution.height;
	if (gpu_resource_flush(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.pMonitorDesc->monitorInfo.framebufferResourceId, flushRect)!=0){
		printf("failed to flush GPU host controller resource framebuffer physical pages to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}	
	*pFontId = fontId;
	while (1){};
	mutex_unlock(&mutex);
	return 0;
}
KAPI int text_subsystem_font_unload(uint64_t fontId){
	if (!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct text_subsystem_font_desc* pFontDesc = (struct text_subsystem_font_desc*)0x00;
	if (subsystem_get_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId, (uint64_t*)&pFontDesc)!=0){
		printf("failed to get text subsystem font descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pFontDesc){
		printf("invalid text subsystem font descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pFontDesc->ppFontNameDescList)
		virtualFree((uint64_t)pFontDesc->ppFontNameDescList, pFontDesc->fontNameDescListSize);
	if (pFontDesc==textSubsystemInfo.fontSubsystemInfo.pLastFontDesc)
		textSubsystemInfo.fontSubsystemInfo.pLastFontDesc = pFontDesc->pBlink;
	if (pFontDesc==textSubsystemInfo.fontSubsystemInfo.pFirstFontDesc)
		textSubsystemInfo.fontSubsystemInfo.pFirstFontDesc = pFontDesc->pFlink;
	if (pFontDesc->pFlink)
		pFontDesc->pFlink->pBlink = pFontDesc->pBlink;
	if (pFontDesc->pBlink)
		pFontDesc->pBlink->pFlink = pFontDesc->pFlink;
	kfree((void*)pFontDesc);
	mutex_unlock(&mutex);
	return 0;
}
KAPI int text_subsystem_font_name_get_desc(uint64_t fontId, uint64_t nameType, struct text_subsystem_font_name_desc** ppFontNameDesc){
	if (!ppFontNameDesc||!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct text_subsystem_font_desc* pFontDesc = (struct text_subsystem_font_desc*)0x00;
	if (subsystem_get_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId, (uint64_t*)&pFontDesc)!=0){
		printf("failed to get text subsystem font descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct text_subsystem_font_name_desc* pFontNameDesc = pFontDesc->ppFontNameDescList[nameType];
	if (!pFontNameDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	*ppFontNameDesc = pFontNameDesc;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int text_subsystem_font_name_type_get_name(uint64_t nameType, const unsigned char** ppNameType){
	if (!ppNameType)
		return -1;
	static const unsigned char* fontNameTypeNameTable[]={
		[FONT_NAME_TYPE_COPYRIGHT]="Copyright",
		[FONT_NAME_TYPE_FAMILY_NAME]="Font family name",
		[FONT_NAME_TYPE_SUBFAMILY_NAME]="Font subfamily name",
		[FONT_NAME_TYPE_UNIQUE_IDENT]="Unique identification",
		[FONT_NAME_TYPE_FULL_NAME]="Full name",
		[FONT_NAME_TYPE_VERSION_STRING]="Version string",
		[FONT_NAME_TYPE_POSTSCRIPT_NAME]="Postscript name",
		[FONT_NAME_TYPE_TRADEMARK]="Trademark",
		[FONT_NAME_TYPE_MANUFACTURER_NAME]="Manufacturer name",
		[FONT_NAME_TYPE_DESIGNER]="Designer name",
		[FONT_NAME_TYPE_DESC]="Description",
		[FONT_NAME_TYPE_URL_VENDOR]="URL Vendor",
		[FONT_NAME_TYPE_URL_DESIGNER]="URL Designer",
		[FONT_NAME_TYPE_LICENSE_DESC]="License description",
		[FONT_NAME_TYPE_LICENSE_INFO_URL]="License info URL",
		[FONT_NAME_TYPE_RESERVED0]="Reserved0",
		[FONT_NAME_TYPE_TYPOGRAPHIC_FAMILY_NAME]="Typographic family name",
		[FONT_NAME_TYPE_TYPOGRAPHIC_SUBFAMILY_NAME]="Typographic subfamily name",
		[FONT_NAME_TYPE_COMPATIBLE_FULL]="MacOS search compatibility name",
		[FONT_NAME_TYPE_SAMPLE_TEXT]="Sample text",
		[FONT_NAME_TYPE_POSTSCRIPT_CID_FONT_LOOKUP_NAME]="Postscript CID font lookup name",
		[FONT_NAME_TYPE_WWS_FAMILY_NAME]="WWS family name",
		[FONT_NAME_TYPE_WWS_SUBFAMILY_NAME]="WWS subfamily name",
		[FONT_NAME_TYPE_LIGHT_BACKGROUND_PALETTE]="Light background palette",
		[FONT_NAME_TYPE_DARK_BACKGROUND_PALETTE]="Dark background palette",
		[FONT_NAME_TYPE_POSTSCRIPT_NAME_PREFIX]="Postscript name prefix",	
	};
	const unsigned char* pNameType = (nameType>FONT_NAME_TYPE_POSTSCRIPT_NAME_PREFIX) ? (const unsigned char*)"Unknown font name type" : fontNameTypeNameTable[nameType];
	*ppNameType = pNameType;
	return 0;
}
int write_pixel(struct uvec2 position, struct uvec4_8 color){
	if (!pbootargs){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (gpu_monitor_exists(1)!=0){
		struct uvec4_8 flip_color = {color.z, color.y, color.x, color.w};
		uint64_t pixelOffset = (position.y*pbootargs->graphicsInfo.width)+position.x;
		struct uvec4_8* pPixel = pbootargs->graphicsInfo.virtualFrameBuffer+pixelOffset;
		*pPixel = flip_color;
		__asm__ volatile("sfence" ::: "memory");
		mutex_unlock(&mutex);
		return 0;
	}
	if (gpu_write_pixel(1, position, color)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;	
}
KAPI int clear(void){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	char_position = 0;
	if (gpu_monitor_exists(1)!=0){
		for (unsigned int i = 0;i<pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height;i++){
			struct uvec2 position = {0};
			position.x = i%pbootargs->graphicsInfo.width;
			position.y = i/pbootargs->graphicsInfo.width;
			write_pixel(position, text_bg);
		}
		mutex_unlock(&mutex);
		return 0;
	}
	struct gpu_monitor_info monitorInfo = {0};
	if (gpu_monitor_get_info(1, &monitorInfo)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(1, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(pMonitorDesc->gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pGpuDesc->gpuInfo.features.acceleration&&pMonitorDesc->monitorInfo.framebufferContextId){
		gpu_cmd_context_reset(pGpuDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId);
		struct gpu_clear_cmd_info clearCmdInfo = {0};
		memset((void*)&clearCmdInfo, 0, sizeof(struct gpu_clear_cmd_info));
		clearCmdInfo.header.commandType = GPU_CMD_TYPE_CLEAR;
		clearCmdInfo.color.x = ((float)text_bg.x)/255.0f;
		clearCmdInfo.color.y = ((float)text_bg.y)/255.0f;
		clearCmdInfo.color.z = ((float)text_bg.z)/255.0f;
		clearCmdInfo.color.w = ((float)text_bg.w)/255.0f;
		clearCmdInfo.depth = 1.0f;
		clearCmdInfo.buffers = (1<<0)|(1<<2);
		gpu_cmd_context_push_cmd(pGpuDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&clearCmdInfo);
		if (gpu_cmd_context_submit(pGpuDesc->gpuId, pMonitorDesc->monitorInfo.framebufferContextId, textSubsystemInfo.accelerationInfo.commandContextId)!=0){
			printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		struct gpu_transfer_from_device_info transferFromDeviceInfo = {0};
		memset((void*)&transferFromDeviceInfo, 0, sizeof(struct gpu_transfer_from_device_info));
		transferFromDeviceInfo.boxRect.x = 0x0;
		transferFromDeviceInfo.boxRect.y = 0x0;
		transferFromDeviceInfo.boxRect.width = pMonitorDesc->monitorInfo.resolution.width;
		transferFromDeviceInfo.boxRect.height = pMonitorDesc->monitorInfo.resolution.height;
		transferFromDeviceInfo.boxRect.depth = 0x01;
		if (gpu_transfer_from_device(pGpuDesc->gpuId, pMonitorDesc->monitorInfo.framebufferResourceId, transferFromDeviceInfo)!=0){
			printf("failed to transfer GPU host controller framebuffer resource data from GPU host controller to graphical text subsystem\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		mutex_unlock(&mutex);
		return 0;	
	}
	for (uint64_t i = 0;i<monitorInfo.resolution.width*monitorInfo.resolution.height;i++){
		struct uvec2 position = {0};
		position.x = i%monitorInfo.resolution.width;
		position.y = i/monitorInfo.resolution.width;
		if (gpu_write_pixel(1, position, text_bg)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
	}
	struct uvec4 syncRect = {0};
	syncRect.x = 0;
	syncRect.y = 0;
	syncRect.z = monitorInfo.resolution.width;
	syncRect.w = monitorInfo.resolution.height;
	if (gpu_sync(1, syncRect)!=0){
		serial_print(0, "failed to flush framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int writechar(unsigned int position, unsigned char ch, unsigned char sync){
	if (ch>255)
		ch = L' ';
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(1)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(1, &monitorInfo)!=0){
			serial_print(0, "failed to get new monitor info\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		resolution = monitorInfo.resolution;
	}
	unsigned int font_offset = ((8*16)/8)*ch;
	unsigned int position_x = position%resolution.width;
	unsigned int position_y = position/resolution.width;
	position_y*=16;
	for (unsigned int x = 0;x<8;x++){
		for (unsigned int y = 0;y<16;y++){
			unsigned int pixelX = position_x+x;
			unsigned int pixelY = position_y+y;
			unsigned int pixel = (pixelY*resolution.width)+pixelX;
			unsigned int font_byte = ((y*8)+x)/8;
			unsigned int font_bit = 8-(((y*8)+x)%8);
			unsigned char* pfontbyte = (unsigned char*)(pbootargs->graphicsInfo.fontData+font_offset+font_byte);
			struct uvec2 position = {0};
			position.x = pixelX;
			position.y = pixelY;
			if ((*pfontbyte)&(1<<font_bit))
				write_pixel(position, text_fg);
			else
				write_pixel(position, text_bg);
		}
	}	
	if (!gpu_monitor_exists(1)&&sync){
		struct uvec4 syncRect = {0};
		syncRect.x = position_x;
		syncRect.y = position_y;
		syncRect.z = 8;
		syncRect.w = 16;
		if (gpu_sync(1, syncRect)!=0){
			serial_print(0, "failed to sync framebuffer\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
	}
	serial_putchar(SERIAL_DEBUG_PORT, (unsigned char)ch);
	mutex_unlock(&mutex);
	return 0;
}
int text_subsystem_putchar(unsigned char ch, unsigned char sync){
	if (!pbootargs->graphicsInfo.font_initialized)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_info monitorInfo = {0};
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(1)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(1, &monitorInfo)!=0){
			printf("failed to get monitor info\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		resolution = monitorInfo.resolution;
	}
	if (char_position>=resolution.width*(resolution.height/16)){
		clear();
	}
	switch (ch){
		case '\n':
		char_position+=pbootargs->graphicsInfo.width;
		char_position-=(char_position%pbootargs->graphicsInfo.width);
		serial_putchar(SERIAL_DEBUG_PORT, '\n');
		mutex_unlock(&mutex);
		return 0;
		case '\r':
		char_position-=(char_position%pbootargs->graphicsInfo.width);
		serial_putchar(SERIAL_DEBUG_PORT, '\r');
		mutex_unlock(&mutex);
		return 0;
		default:
		break;
		case '\b':
		if (char_position<8)
			break;
		char_position-=8;
		writechar(char_position, L' ', sync);
		serial_putchar(SERIAL_DEBUG_PORT, '\b');
		mutex_unlock(&mutex);
		return 0;
	}
	writechar(char_position, ch, sync);	
	char_position+=8;
	mutex_unlock(&mutex);
	return 0;
}
int text_subsystem_print(unsigned char* pString, unsigned char sync){
	if (!pString)
		return -1;
	for (uint64_t i = 0;pString[i];i++){
		text_subsystem_putchar(pString[i], 0x01);
	}
	return 0;
}
KAPI int putchar(unsigned char ch){
	return text_subsystem_putchar(ch, 0x01);
}
KAPI int putlchar(uint16_t ch){
	if (putchar((unsigned char)ch)!=0)
		return -1;
	return 0;
}
int puthex(unsigned char hex, unsigned char isUpper){
	if (hex>16)
		return -1;
	if (hex<10){
		putchar(L'0'+hex);
		return 0;
	}
	if (isUpper)
		putchar(L'A'+hex-10);
	else
		putchar(L'a'+hex-10);
	return 0;
}
KAPI int print(unsigned char* string){
	if (!string)
		return -1;
	return text_subsystem_print(string, 0x01);
}
KAPI int lprint(uint16_t* lstring){
	if (!lstring)
		return -1;
	for (unsigned int i = 0;lstring[i];i++){
		text_subsystem_putchar((unsigned char)lstring[i], 0x01);
	}
	return 0;
}
KAPI int set_text_color(struct uvec4_8 fg, struct uvec4_8 bg){
	text_fg = fg;
	text_bg = bg;
	return 0;
}
KAPI int get_text_color(struct uvec4_8* pFg, struct uvec4_8* pBg){
	if (pFg)
		*pFg = text_fg;
	if (pBg)
		*pBg = text_bg;
	return 0;
}
