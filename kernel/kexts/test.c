#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "cpu/thread.h"
#include "math/basic.h"
#include "math/trig.h"
#include "math/matrix.h"
#include "subsystem/text.h"
#include "subsystem/filesystem.h"
#include "subsystem/gpu.h"
#include "mem/vmm.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "kexts/test.h"
__attribute__((ms_abi)) int kext_entry(uint64_t pid){
	uint64_t time_us = get_time_us();
	printf("pid: %d\r\n", pid);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_exists(0x01)!=0){
		printf("GPU host controller lacks GPU host controller driver\r\n");
		return -1;
	}
	if (gpu_monitor_get_desc(0x01, &pMonitorDesc)!=0){
		printf("failed to get GPU host controller monitor descriptor\r\n");
		return -1;
	}
	uint64_t gpuId = pMonitorDesc->gpuId;
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		return -1;
	}
	if (!pGpuDesc->gpuInfo.features.acceleration){
		printf("GPU host controller lacks support for graphical acceleration\r\n");
		return -1;
	}
	uint64_t contextId = pMonitorDesc->monitorInfo.framebufferContextId;
	uint64_t cmdContextId = 0x00;
	if (gpu_cmd_context_init(gpuId, PAGE_SIZE*4, &cmdContextId)!=0){
		printf("failed to initialize GPU host controller command context via GPU subsystem\r\n");
		return -1;
	}
	gpu_cmd_context_reset(gpuId, cmdContextId);
	uint64_t surfaceObjectId = 0x00;
	struct gpu_create_surface_object_info createSurfaceObjectInfo = {0};
	memset((void*)&createSurfaceObjectInfo, 0, sizeof(struct gpu_create_surface_object_info));
	createSurfaceObjectInfo.header.objectType = GPU_OBJECT_TYPE_SURFACE;
	createSurfaceObjectInfo.resourceId = pMonitorDesc->monitorInfo.framebufferResourceId;
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createSurfaceObjectInfo, &surfaceObjectId)!=0){
		printf("failed to create GPU host controller surface object\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	gpu_cmd_context_reset(gpuId, cmdContextId);
	uint32_t colorBufferHandle = (uint32_t)surfaceObjectId;
	struct gpu_set_framebuffer_state_list_cmd_info setFramebufferStateListCmdInfo = {0};
	memset((void*)&setFramebufferStateListCmdInfo, 0, sizeof(struct gpu_set_framebuffer_state_list_cmd_info));
	setFramebufferStateListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_FRAMEBUFFER_STATE_LIST;
	setFramebufferStateListCmdInfo.colorBufferCount = 0x01;
	setFramebufferStateListCmdInfo.pColorBufferHandleList = (uint32_t*)&colorBufferHandle;
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setFramebufferStateListCmdInfo);
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
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setViewportStateListCmdInfo);
	struct gpu_set_scissor_state_list_cmd_info setScissorStateListCmdInfo = {0};
	memset((void*)&setScissorStateListCmdInfo, 0, sizeof(struct gpu_set_scissor_state_list_cmd_info));
	struct gpu_scissor_state scissorStateList[1];
	memset((void*)scissorStateList, 0, sizeof(struct gpu_scissor_state));
	scissorStateList[0].maxX = pMonitorDesc->monitorInfo.resolution.width;
	scissorStateList[0].maxY = pMonitorDesc->monitorInfo.resolution.height;
	setScissorStateListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_SCISSOR_STATE_LIST;
	setScissorStateListCmdInfo.scissorStateCount = 0x01;
	setScissorStateListCmdInfo.pScissorStateList = (struct gpu_scissor_state*)scissorStateList;
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setScissorStateListCmdInfo);
	if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
		printf("failed to submit GPU host controller command context via GPU subsystem\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	struct gpu_create_rasterizer_state_object_info createRasterizerStateObjectInfo = {0};
	memset((void*)&createRasterizerStateObjectInfo, 0, sizeof(struct gpu_create_rasterizer_state_object_info));
	struct gpu_rasterizer_state rasterizerState = {0};
	memset((void*)&rasterizerState, 0, sizeof(struct gpu_rasterizer_state));
	rasterizerState.depth_clip = 1;
	rasterizerState.scissor = 1;
	rasterizerState.front_ccw = 1;
	rasterizerState.half_pixel_center = 1;
	rasterizerState.bottom_edge_rule = 1;
	rasterizerState.line_smooth = 1;
	rasterizerState.fill_mode_front = GPU_POLYGON_MODE_FILL;
	rasterizerState.fill_mode_back = GPU_POLYGON_MODE_FILL;
	rasterizerState.cull_face_mode = GPU_FACE_BACK;
	rasterizerState.point_size = 4.0f;
	rasterizerState.line_width = 4.0f;
	createRasterizerStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_RASTERIZER_STATE;
	createRasterizerStateObjectInfo.surfaceObjectId = surfaceObjectId;
	createRasterizerStateObjectInfo.pRasterizerState = &rasterizerState;
	struct gpu_create_dsa_state_object_info createDsaStateObjectInfo = {0};
	memset((void*)&createDsaStateObjectInfo, 0, sizeof(struct gpu_create_dsa_state_object_info));
	struct gpu_dsa_state dsaState = {0};
	memset((void*)&dsaState, 0, sizeof(struct gpu_dsa_state));
	dsaState.depth_enable = 0x01;
	dsaState.depth_func = GPU_FUNC_LEQUAL;
	dsaState.alpha_enable = 0x01;
	dsaState.alpha_func = GPU_FUNC_ALWAYS;
	createDsaStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_DSA_STATE;
	createDsaStateObjectInfo.surfaceObjectId = surfaceObjectId;
	createDsaStateObjectInfo.pDsaState = &dsaState;
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
	createBlendStateListObjectInfo.blendStateCount = 0x01;
	createBlendStateListObjectInfo.pBlendStateList = (struct gpu_blend_state*)blendStateList;
	uint64_t rasterizerStateObjectId = 0x00;
	uint64_t dsaStateObjectId = 0x00;
	uint64_t blendStateObjectId = 0x00;
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createRasterizerStateObjectInfo, &rasterizerStateObjectId)!=0){
		printf("failed to create GPU host controller rasterizer state object\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createDsaStateObjectInfo, &dsaStateObjectId)!=0){
		printf("failed to create GPU host controller DSA state object\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createBlendStateListObjectInfo, &blendStateObjectId)!=0){
		printf("failed to create GPU host controller blend state object\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	struct gpu_vertex_buffer_cube_face* pVertexBuffer = (struct gpu_vertex_buffer_cube_face*)0x00;
	uint64_t vertexBufferSize = sizeof(struct gpu_vertex_buffer_cube_face);
	if (virtualAlloc((uint64_t*)&pVertexBuffer, vertexBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for GPU host controller triangle vertex buffer\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	memset((void*)pVertexBuffer, 0, vertexBufferSize);
	static const struct fvec4_32 vertexPositionList[24]={
		{-1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f, 1.0f},
		{-1.0f, 1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, -1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f, 1.0f},
		{1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f, 1.0f},
	};
	static const struct fvec4_32 vertexColorList[4]={
		{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
	};
	for (uint64_t i = 0;i<sizeof(pVertexBuffer->vertex_list)/sizeof(pVertexBuffer->vertex_list[0]);i++){
		struct fvec4_32 vertexPosition = vertexPositionList[i];
		struct fvec4_32 vertexColor = vertexColorList[i%4];
		pVertexBuffer->vertex_list[i].position = vertexPosition;
		pVertexBuffer->vertex_list[i].color = vertexColor;
	}
	uint64_t vertexBufferResourceId = 0x00;
	struct gpu_create_resource_info createResourceInfo = {0};
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32_FLOAT;
	createResourceInfo.resourceInfo.normal.format = 64;
	createResourceInfo.resourceInfo.normal.width = vertexBufferSize;
	createResourceInfo.resourceInfo.normal.height = 0x01;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.arraySize = 0x01;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_VERTEX_BUFFER;
	createResourceInfo.resourceInfo.normal.flags = 0x00;
	if (gpu_resource_create(gpuId, createResourceInfo, &vertexBufferResourceId)!=0){
		printf("failed to create GPU host controller vertex buffer object resource\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_context_attach_resource(gpuId, contextId, vertexBufferResourceId)!=0){
		printf("failed to attach GPU host controller vertex buffer resource to GPU host controller context\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_resource_attach_backing(gpuId, vertexBufferResourceId, (unsigned char*)pVertexBuffer, vertexBufferSize)!=0){
		printf("failed to attach physical pages to GPU host controller vertex buffer resource\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	unsigned char* pIndexBuffer = (unsigned char*)0x0;
	uint64_t indexBufferSize = sizeof(uint8_t)*36;
	if (virtualAlloc((uint64_t*)&pIndexBuffer, indexBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for GPU host controller index buffer\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	static uint8_t indexBuffer[36]={
		0, 1, 2,
		2, 3, 0,

		4, 5, 6,
		6, 7, 4,
		
		8, 9, 10,
		10, 11, 8,

		12, 13, 14,
		14, 15, 12,

		16, 17, 18,
		18, 19, 16,

		20, 21, 22,
		22, 23, 20,
	};
	memcpy((void*)pIndexBuffer, (void*)indexBuffer, indexBufferSize);
	uint64_t indexBufferResourceId = 0x00;
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.width = indexBufferSize;
	createResourceInfo.resourceInfo.normal.height = 0x01;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_INDEX_BUFFER;
	createResourceInfo.resourceInfo.normal.flags = 0x00;
	if (gpu_resource_create(gpuId, createResourceInfo, &indexBufferResourceId)!=0){
		printf("failed to create GPU host controller index buffer resource\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_context_attach_resource(gpuId, contextId, indexBufferResourceId)!=0){
		printf("failed to attach GPU host controller index buffer resource to GPU host controller context\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_resource_attach_backing(gpuId, indexBufferResourceId, (unsigned char*)pIndexBuffer, indexBufferSize)!=0){
		printf("failed to attach physical pages to GPU host controller index buffer resource\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	uint64_t vertexElementListObjectId = 0x00;
	struct gpu_vertex_element vertexElementList[2] = {0};
	memset((void*)&vertexElementList, 0, sizeof(struct gpu_vertex_element)*2);
	vertexElementList[0].src_offset = 0x00;
	vertexElementList[0].vertex_buffer_index = 0x00;
	vertexElementList[0].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
	vertexElementList[1].src_offset = sizeof(struct fvec4_32);
	vertexElementList[1].vertex_buffer_index = 0x00;
	vertexElementList[1].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
	struct gpu_create_vertex_element_list_object_info createVertexElementListObjectInfo = {0};
	memset((void*)&createVertexElementListObjectInfo, 0, sizeof(struct gpu_create_vertex_element_list_object_info));
	createVertexElementListObjectInfo.header.objectType = GPU_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
	createVertexElementListObjectInfo.surfaceObjectId = surfaceObjectId;
	createVertexElementListObjectInfo.vertexElementCount = 0x02;
	createVertexElementListObjectInfo.pVertexElementList = (struct gpu_vertex_element*)vertexElementList;
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createVertexElementListObjectInfo, &vertexElementListObjectId)!=0){
		printf("failed to create GPU host controller vertex element list object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	struct gpu_transfer_to_device_info transferToDeviceInfo = {0};
	memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
	transferToDeviceInfo.boxRect.width = vertexBufferSize;
	transferToDeviceInfo.boxRect.height = 0x01;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(gpuId, vertexBufferResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller vertex buffer resource physical pages to GPU host controller\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	transferToDeviceInfo.boxRect.width = indexBufferSize;
	transferToDeviceInfo.boxRect.height = 0x01;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(gpuId, indexBufferResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller index buerf resource physical pages to GPU host controller\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	struct uvec4_8* pTextureBuffer = (struct uvec4_8*)0x00;
	struct uvec4_32 textureResolution = {0};
	textureResolution.x = 256;
	textureResolution.y = 256;
	uint64_t textureBufferSize = textureResolution.x*textureResolution.y*sizeof(struct uvec4_8);
	if (virtualAlloc((uint64_t*)&pTextureBuffer, textureBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for GPU host controller texture\r\n");
		virtualFree((uint64_t)pVertexBuffer,  vertexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	struct uvec4_8 normalTextureColor = {0};
	normalTextureColor.x = 0x00;
	normalTextureColor.y = 0x00;
	normalTextureColor.z = 0x00;
	normalTextureColor.w = 0xFF;
	struct uvec4_8 flipTextureColor = {0};
	flipTextureColor.x = 0xFF;
	flipTextureColor.y = 0xFF;
	flipTextureColor.z = 0x00;
	flipTextureColor.w = 0xFF;
	for (uint64_t i = 0;i<textureResolution.x*textureResolution.y;i++){
		uint8_t flipColor = (i/8)%2;
		flipColor = ((i/textureResolution.x)%2);
		pTextureBuffer[i] = (flipColor ? flipTextureColor : normalTextureColor);
	}
	const unsigned char vertexShader[]="VERT\n"
	"DCL OUT[0], POSITION\n"
	"DCL OUT[1], COLOR\n"
	"DCL IN[0]\n"
	"DCL IN[1]\n"
	"DCL CONST[0..3]\n"
	"DCL TEMP[0]\n"
	"DP4 TEMP[0].x, CONST[0], IN[0]\n"
	"DP4 TEMP[0].y, CONST[1], IN[0]\n"
	"DP4 TEMP[0].z, CONST[2], IN[0]\n"
	"DP4 TEMP[0].w, CONST[3], IN[0]\n"
	"MOV OUT[0], TEMP[0]\n"
	"MOV OUT[1], IN[1]\n"
	"END\n";
	const unsigned char fragmentShader[]="FRAG\n"
	"DCL OUT[0], COLOR\n"
	"DCL IN[0], COLOR, PERSPECTIVE\n"
	"MOV OUT[0], IN[0]\n"
	"END\n";
	uint64_t vertexShaderObjectId = 0x00;
	uint64_t fragmentShaderObjectId = 0x00;
	struct gpu_create_shader_object_info createShaderObjectInfo = {0};
	memset((void*)&createShaderObjectInfo, 0, sizeof(struct gpu_create_shader_object_info));
	createShaderObjectInfo.header.objectType = GPU_OBJECT_TYPE_SHADER;
	createShaderObjectInfo.surfaceObjectId = surfaceObjectId;
	createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
	createShaderObjectInfo.pShaderCode = (unsigned char*)vertexShader;
	createShaderObjectInfo.shaderCodeSize = sizeof(vertexShader);
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &vertexShaderObjectId)!=0){
		printf("failed to create GPU host controller vertex shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
	createShaderObjectInfo.pShaderCode = (unsigned char*)fragmentShader;
	createShaderObjectInfo.shaderCodeSize = sizeof(fragmentShader);
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &fragmentShaderObjectId)!=0){
		printf("failed to create GPU host controller fragment shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	gpu_cmd_context_reset(gpuId, cmdContextId);
	struct gpu_set_vertex_buffer_list_cmd_info setVertexBufferListCmdInfo = {0};
	memset((void*)&setVertexBufferListCmdInfo, 0, sizeof(struct gpu_set_vertex_buffer_list_cmd_info));
	struct gpu_vertex_buffer vertexBufferList[1] = {0};
	memset((void*)vertexBufferList, 0, sizeof(struct gpu_vertex_buffer));
	vertexBufferList[0].stride = sizeof(struct gpu_vertex_basic);
	vertexBufferList[0].resource_id = vertexBufferResourceId;
	setVertexBufferListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VERTEX_BUFFER_LIST;
	setVertexBufferListCmdInfo.vertexBufferCount = 0x01;
	setVertexBufferListCmdInfo.pVertexBufferList = (struct gpu_vertex_buffer*)vertexBufferList;
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setVertexBufferListCmdInfo);
	if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, rasterizerStateObjectId)!=0){
		printf("failed to bind GPU host cotnroller rasterizer state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, dsaStateObjectId)!=0){
		printf("failed to bind GPU host controller DSA state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_reset(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, blendStateObjectId)!=0){
		printf("failed to bind GPU host controller blend state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);	
		return -1;
	}
	if (gpu_object_bind(gpuId, vertexElementListObjectId)!=0){
		printf("failed to bind GPU host controller vertex element list object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, vertexShaderObjectId)!=0){
		printf("failed to bind GPU host controller vertex shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, fragmentShaderObjectId)!=0){
		printf("failed to bind GPU host controller fragment shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	gpu_cmd_context_reset(gpuId, cmdContextId);
	struct gpu_set_index_buffer_cmd_info setIndexBufferCmdInfo = {0};
	memset((void*)&setIndexBufferCmdInfo, 0, sizeof(struct gpu_set_index_buffer_cmd_info));
	setIndexBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_INDEX_BUFFER;
	setIndexBufferCmdInfo.resourceId = indexBufferResourceId;
	setIndexBufferCmdInfo.length = sizeof(uint8_t);
	setIndexBufferCmdInfo.offset = 0x00;
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setIndexBufferCmdInfo);
	if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pIndexBuffer, indexBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	clear();
	for (uint64_t frameCount = 0;;frameCount++){
		static double aspect = 0.0;
		if (!frameCount)
			aspect = (double)(((double)pMonitorDesc->monitorInfo.resolution.width)/((double)pMonitorDesc->monitorInfo.resolution.height));
		static double verticalFieldOfView = 90.0;
		static double far = 1000.0;
		static double near = 0.01;
		static struct fvec3_32 vertexTranslate = {0.0f, 0.0f, 0.0f};
		static struct fvec3_32 vertexScale = {0.5f, 0.5f, 0.5f};
		static struct fvec3_32 vertexRotate = {0.0f, 0.0f, 0.0f};
		static struct fvec3_32 cameraPosition = {1.0f, 1.0f, -1.0f};
		static struct fvec3_32 cameraTargetPosition = {0.0f, 0.0f, 0.0f};
		static struct fvec3_32 cameraLookupPosition = {0.0f, 1.0f, 0.0f};
		struct fvec3_32 cameraForwardVector = {0};
		struct fvec3_32 cameraRightVector = {0};
		struct fvec3_32 cameraUpVector = {0};
		vertexRotate.y = modf(((double)get_time_us())*0.000001, PI2_F);
		cameraForwardVector = normf3_32(fvec3_32_sub(cameraTargetPosition, cameraPosition));
		cameraRightVector = normf3_32(crossf3_32(cameraForwardVector, cameraLookupPosition));
		cameraUpVector = crossf3_32(cameraForwardVector, cameraRightVector);
		float viewMatrix[16] = {0};
		memset((void*)viewMatrix, 0, sizeof(float)*16);
		viewMatrix[0] = cameraRightVector.x;
		viewMatrix[4] = cameraRightVector.y;
		viewMatrix[8] = cameraRightVector.z;
		viewMatrix[1] = cameraUpVector.x;
		viewMatrix[5] = cameraUpVector.y;
		viewMatrix[9] = cameraUpVector.z;
		viewMatrix[2] = cameraForwardVector.x;
		viewMatrix[6] = cameraForwardVector.y;
		viewMatrix[10] = cameraForwardVector.z;
		viewMatrix[12] = -((cameraRightVector.x*cameraPosition.x)+(cameraRightVector.y*cameraPosition.y)+(cameraRightVector.z*cameraPosition.z));
		viewMatrix[13] = -((cameraUpVector.x*cameraPosition.x)+(cameraUpVector.y*cameraPosition.y)+(cameraUpVector.z*cameraPosition.z));
		viewMatrix[14] = ((cameraForwardVector.x*cameraPosition.x)+(cameraForwardVector.y*cameraPosition.y)+(cameraForwardVector.z*cameraPosition.z));
		viewMatrix[15] = 1.0f;
		float projectionMatrix[16] = {0};
		memset((void*)projectionMatrix, 0, sizeof(float)*16);
		projectionMatrix[0] = (float)(1.0/(aspect*tanf(verticalFieldOfView*0.5)));
		projectionMatrix[5] = (float)(1.0/(tanf(verticalFieldOfView*0.5)));
		projectionMatrix[10] = -(float)((far+near)/(far-near));
		projectionMatrix[11] = -1.0f;
		projectionMatrix[14] = -(float)((2.0f*far*near)/(far-near));
		float translationMatrix[16] = {0};
		memset((void*)translationMatrix, 0, sizeof(float)*16);
		translationMatrix[0] = 1.0f;
		translationMatrix[5] = 1.0f;
		translationMatrix[10] = 1.0f;
		translationMatrix[3] = vertexTranslate.x;
		translationMatrix[7] = vertexTranslate.y;
		translationMatrix[11] = vertexTranslate.z;
		translationMatrix[15] = 1.0f;
		float scaleMatrix[16] = {0};
		memset((void*)scaleMatrix, 0, sizeof(float)*16);
		scaleMatrix[0] = vertexScale.x;
		scaleMatrix[5] = vertexScale.y;
		scaleMatrix[10] = vertexScale.z;
		scaleMatrix[15] = 1.0f;
		float rotationMatrixX[16] = {0};
		memset((void*)rotationMatrixX, 0, sizeof(float)*16);
		rotationMatrixX[0] = 1.0f;
		rotationMatrixX[5] = cosf(vertexRotate.x);
		rotationMatrixX[6] = -sinf(vertexRotate.x);
		rotationMatrixX[9] = sinf(vertexRotate.x);
		rotationMatrixX[10] = cosf(vertexRotate.x);
		rotationMatrixX[15] = 1.0f;
		float rotationMatrixY[16] = {0};
		memset((void*)rotationMatrixY, 0, sizeof(float)*16);
		rotationMatrixY[0] = cosf(vertexRotate.y);
		rotationMatrixY[2] = sinf(vertexRotate.y);
		rotationMatrixY[5] = 1.0f;
		rotationMatrixY[8] = -sinf(vertexRotate.y);
		rotationMatrixY[10] = cosf(vertexRotate.y);
		rotationMatrixY[15] = 1.0f;
		float rotationMatrixZ[16] = {0};
		memset((void*)rotationMatrixZ, 0, sizeof(float)*16);
		rotationMatrixZ[0] = cosf(vertexRotate.z);
		rotationMatrixZ[1] = -sinf(vertexRotate.z);
		rotationMatrixZ[4] = sinf(vertexRotate.z);
		rotationMatrixZ[5] = cosf(vertexRotate.z);
		rotationMatrixZ[10] = 1.0f;
		rotationMatrixZ[15] = 1.0f;
		float rotationMatrix[16] = {0};
		float tempMatrix[16] = {0};
		matrix4x4_f32_multiply((float*)rotationMatrixY, (float*)rotationMatrixX, (float*)tempMatrix);
		matrix4x4_f32_multiply((float*)rotationMatrixZ, (float*)tempMatrix, (float*)rotationMatrix);
		float modelMatrix[16] = {0};
		matrix4x4_f32_multiply((float*)rotationMatrix, (float*)scaleMatrix, (float*)tempMatrix);
		matrix4x4_f32_multiply((float*)tempMatrix, (float*)translationMatrix, (float*)modelMatrix);
		float modelViewProjectionMatrix[16] = {0};
		matrix4x4_f32_multiply((float*)viewMatrix, (float*)modelMatrix, (float*)tempMatrix);
		matrix4x4_f32_multiply((float*)projectionMatrix, (float*)tempMatrix, (float*)modelViewProjectionMatrix);
		matrix4x4_f32_transpose((float*)modelViewProjectionMatrix, (float*)modelViewProjectionMatrix);
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_set_constant_buffer_cmd_info setConstantBufferCmdInfo = {0};
		memset((void*)&setConstantBufferCmdInfo, 0, sizeof(struct gpu_set_constant_buffer_cmd_info));
		setConstantBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_CONSTANT_BUFFER;
		setConstantBufferCmdInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
		setConstantBufferCmdInfo.index = 0x00;
		setConstantBufferCmdInfo.bufferSize = sizeof(float)*16;
		setConstantBufferCmdInfo.pBuffer = (unsigned char*)modelViewProjectionMatrix;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setConstantBufferCmdInfo);
		if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
			printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
			virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			gpu_cmd_context_deinit(gpuId, cmdContextId);
			return -1;
		}
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_clear_cmd_info clearCmdInfo = {0};
		memset((void*)&clearCmdInfo, 0, sizeof(struct gpu_clear_cmd_info));
		clearCmdInfo.header.commandType = GPU_CMD_TYPE_CLEAR;
		clearCmdInfo.color.x = 0.25f;
		clearCmdInfo.color.y = 0.25f;
		clearCmdInfo.color.z = 0.25f;
		clearCmdInfo.color.w = 1.0f;
		clearCmdInfo.depth = 1.0f;
		clearCmdInfo.buffers = (1<<0)|(1<<2);
		struct gpu_draw_vbo_cmd_info drawVboCmdInfo = {0};
		memset((void*)&drawVboCmdInfo, 0, sizeof(struct gpu_draw_vbo_cmd_info));
		struct gpu_draw_vbo_info drawVboInfo = {0};
		memset((void*)&drawVboInfo, 0, sizeof(struct gpu_draw_vbo_info));
		drawVboInfo.start = 0x00;
		drawVboInfo.count = indexBufferSize;
		drawVboInfo.mode = GPU_PRIMITIVE_TRIANGLES;
		drawVboInfo.instance_count = 0x01;
		drawVboInfo.index_size = indexBufferSize;
		drawVboInfo.max_index = drawVboInfo.count-0x01;
		drawVboCmdInfo.header.commandType = GPU_CMD_TYPE_DRAW_VBO;
		drawVboCmdInfo.pDrawVboInfo = &drawVboInfo;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&clearCmdInfo);
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&drawVboCmdInfo);
		if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
			printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
			virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			gpu_cmd_context_deinit(gpuId, cmdContextId);
			return -1;
		}
		struct gpu_rect flushRect = {0};
		flushRect.x = 0x00;
		flushRect.y = 0x00;
		flushRect.width = pMonitorDesc->monitorInfo.resolution.width;
		flushRect.height = pMonitorDesc->monitorInfo.resolution.height;
		if (gpu_resource_flush(gpuId, pMonitorDesc->monitorInfo.framebufferResourceId, flushRect)!=0){
			printf("failed to flush framebuffer physical pages to GPU host controller via GPU subsystem\r\n");
			virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			gpu_cmd_context_deinit(gpuId, cmdContextId);
			return -1;
		}
		break;
	}
	struct gpu_transfer_from_device_info transferFromDeviceInfo = {0};
	memset((void*)&transferFromDeviceInfo, 0, sizeof(struct gpu_transfer_from_device_info));
	transferFromDeviceInfo.boxRect.width = pMonitorDesc->monitorInfo.resolution.width;
	transferFromDeviceInfo.boxRect.height = pMonitorDesc->monitorInfo.resolution.height;
	transferFromDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_from_device(gpuId, pMonitorDesc->monitorInfo.framebufferResourceId, transferFromDeviceInfo)!=0){
		printf("failed to transfer GPU host controller framebuffer resource physical pages to graphical program via GPU subsystem\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
	virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
	gpu_resource_delete(gpuId, vertexBufferResourceId);
	gpu_resource_delete(gpuId, indexBufferResourceId);
	gpu_cmd_context_deinit(gpuId, cmdContextId);
	uint64_t fontFileId = 0x00;
	struct fs_file_info fontFileInfo = {0};
	if (fs_open(0x01, "FONTS/HERSHEY.TTF", 0x00, &fontFileId)!=0){
		printf("failed to open GPU host controller font file\r\n");
		return -1;
	}
	if (fs_getFileInfo(0x01, fontFileId, &fontFileInfo)!=0){
		printf("failed to get GPU host controller font file info\r\n");
		fs_close(0x01, fontFileId);
		return -1;
	}
	unsigned char* pFontBuffer = (unsigned char*)0x00;
	uint64_t fontBufferSize = fontFileInfo.fileSize;
	if (virtualAlloc((uint64_t*)&pFontBuffer, fontBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for GPU host controller font physical pages\r\n");
		fs_close(0x01, fontFileId);
		return -1;
	}
	if (fs_read(0x01, fontFileId, pFontBuffer, fontBufferSize)!=0){
		printf("failed to read GPU host controller font physical page data from non-volatile memory\r\n");
		virtualFree((uint64_t)pFontBuffer, fontBufferSize);
		fs_close(0x01, fontFileId);
		return -1;
	}
	if (fs_write(0x01, fontFileId, pFontBuffer, fontBufferSize)!=0){
		printf("failed to write GPU Host controller truetype font physical pages to non-volatile memory\r\n");
		virtualFree((uint64_t)pFontBuffer, fontBufferSize);
		fs_close(0x01, fontFileId);
		return -1;
	}
	fs_close(0x01, fontFileId);
	printf("GPU host controller font physical pages: %d\r\n", fontBufferSize/PAGE_SIZE);
	uint64_t fontId = 0x00;
	if (text_subsystem_font_load(pFontBuffer, fontBufferSize, &fontId)!=0){
		printf("failed to load GPU host controller font\r\n");
		virtualFree((uint64_t)pFontBuffer, fontBufferSize);
		return -1;
	}
	for (uint64_t i = 0;i<25;i++){
		struct text_subsystem_font_name_desc* pFontNameDesc = (struct text_subsystem_font_name_desc*)0x00;
		if (text_subsystem_font_name_get_desc(fontId, i, &pFontNameDesc)!=0){
			continue;
		}
		const unsigned char* pFontNameTypeName = (const unsigned char*)"Unknown font name type";
		text_subsystem_font_name_type_get_name(i, &pFontNameTypeName);
		printf("%s: %s\r\n", pFontNameTypeName, pFontNameDesc->name);
	}
	if (text_subsystem_font_unload(fontId)!=0){
		printf("failed to unload GPU host controller font\r\n");
		virtualFree((uint64_t)pFontBuffer, fontBufferSize);
		return -1;
	}
	virtualFree((uint64_t)pFontBuffer, fontBufferSize);
	while (1){};
	return 0;
}
