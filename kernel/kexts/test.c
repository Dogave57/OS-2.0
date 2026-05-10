#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "cpu/thread.h"
#include "subsystem/text.h"
#include "subsystem/gpu.h"
#include "mem/vmm.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
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
	while (1){};
	printf("GPU host controller master context ID: %d\r\n", contextId);
	printf("GPU host controller cmd context ID: %d\r\n", cmdContextId);
	uint64_t surfaceObjectId = pMonitorDesc->monitorInfo.framebufferSurfaceObjectId;
	struct gpu_create_rasterizer_state_object_info createRasterizerStateObjectInfo = {0};
	memset((void*)&createRasterizerStateObjectInfo, 0, sizeof(struct gpu_create_rasterizer_state_object_info));
	struct gpu_rasterizer_state rasterizerState = {0};
	memset((void*)&rasterizerState, 0, sizeof(struct gpu_rasterizer_state));
//	rasterizerState.depth_clip = 1;
	rasterizerState.scissor = 1;
	rasterizerState.front_ccw = 1;
	rasterizerState.half_pixel_center = 1;
	rasterizerState.bottom_edge_rule = 1;
	rasterizerState.line_smooth = 1;
	rasterizerState.fill_mode_front = GPU_POLYGON_MODE_FILL;
	rasterizerState.fill_mode_back = GPU_POLYGON_MODE_FILL;
	rasterizerState.point_size = 4.0f;
	rasterizerState.line_width = 4.0f;
	createRasterizerStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_RASTERIZER_STATE;
	createRasterizerStateObjectInfo.surfaceObjectId = surfaceObjectId;
	createRasterizerStateObjectInfo.pRasterizerState = &rasterizerState;
	struct gpu_create_dsa_state_object_info createDsaStateObjectInfo = {0};
	memset((void*)&createDsaStateObjectInfo, 0, sizeof(struct gpu_create_dsa_state_object_info));
	struct gpu_dsa_state dsaState = {0};
	memset((void*)&dsaState, 0, sizeof(struct gpu_dsa_state));
//	dsaState.depth_enable = 0x01;
//	dsaState.depth_func = GPU_FUNC_LEQUAL;
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
	struct gpu_vertex_buffer_triangle* pVertexBuffer = (struct gpu_vertex_buffer_triangle*)0x00;
	uint64_t vertexBufferSize = sizeof(struct gpu_vertex_buffer_triangle);
	if (virtualAllocPage((uint64_t*)&pVertexBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for GPU host controller triangle vertex buffer\r\n");
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	memset((void*)pVertexBuffer, 0, vertexBufferSize);
	pVertexBuffer->vertex_list[0].position.x = -0.25f;
	pVertexBuffer->vertex_list[0].position.y = 0.0f;
	pVertexBuffer->vertex_list[0].position.z = 0.0f;
	pVertexBuffer->vertex_list[0].position.w = 1.0f;
	pVertexBuffer->vertex_list[1].position.x = 0.0f;
	pVertexBuffer->vertex_list[1].position.y = 0.25f;
	pVertexBuffer->vertex_list[1].position.z = 0.0f;
	pVertexBuffer->vertex_list[1].position.w = 1.0f;
	pVertexBuffer->vertex_list[2].position.x = 0.25f;
	pVertexBuffer->vertex_list[2].position.y = 0.0f;
	pVertexBuffer->vertex_list[2].position.z = 0.0f;
	pVertexBuffer->vertex_list[2].position.w = 1.0f;
	pVertexBuffer->vertex_list[0].color.x = 0.5f;
	pVertexBuffer->vertex_list[0].color.y = 0.0f;
	pVertexBuffer->vertex_list[0].color.z = 0.0f;
	pVertexBuffer->vertex_list[0].color.w = 1.0f;
	pVertexBuffer->vertex_list[1].color.x = 0.0f;
	pVertexBuffer->vertex_list[1].color.y = 1.0f;
	pVertexBuffer->vertex_list[1].color.z = 0.0f;
	pVertexBuffer->vertex_list[1].color.w = 1.0f;
	pVertexBuffer->vertex_list[2].color.x = 0.0f;
	pVertexBuffer->vertex_list[2].color.y = 0.0f;
	pVertexBuffer->vertex_list[2].color.z = 1.0f;
	pVertexBuffer->vertex_list[2].color.w = 1.0f;
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
	if (gpu_resource_attach_backing(gpuId, vertexBufferResourceId, (unsigned char*)pVertexBuffer, PAGE_SIZE)!=0){
		printf("failed to attach physical pages to GPU host controller vertex buffer resource\r\n");
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
	uint64_t vertexElementListObjectId = 0x00;
	struct gpu_vertex_element vertexElementList[2] = {0};
	memset((void*)&vertexElementList, 0, sizeof(struct gpu_vertex_element)*2);
	vertexElementList[0].src_offset = 0x00;
	vertexElementList[0].vertex_buffer_index = 0x00;
	vertexElementList[0].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
	vertexElementList[1].src_offset = sizeof(pVertexBuffer->vertex_list[0].position);
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
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	const unsigned char vertexShader[]="VERT\n"
	"DCL OUT[0], POSITION\n"
	"DCL OUT[1], COLOR\n"
	"DCL IN[0]\n"
	"DCL IN[1]\n"
	"MOV OUT[0], IN[0]\n"
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
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
	createShaderObjectInfo.pShaderCode = (unsigned char*)fragmentShader;
	createShaderObjectInfo.shaderCodeSize = sizeof(fragmentShader);
	if (gpu_object_create(gpuId, contextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &fragmentShaderObjectId)!=0){
		printf("failed to create GPU host controller fragment shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
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
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, rasterizerStateObjectId)!=0){
		printf("failed to bind GPU host cotnroller rasterizer state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, dsaStateObjectId)!=0){
		printf("failed to bind GPU host controller DSA state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_reset(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, blendStateObjectId)!=0){
		printf("failed to bind GPU host controller blend state object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);	
		return -1;
	}
	if (gpu_object_bind(gpuId, vertexElementListObjectId)!=0){
		printf("failed to bind GPU host controller vertex element list object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, vertexShaderObjectId)!=0){
		printf("failed to bind GPU host controller vertex shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	if (gpu_object_bind(gpuId, fragmentShaderObjectId)!=0){
		printf("failed to bind GPU host controller fragment shader object\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	gpu_cmd_context_reset(gpuId, cmdContextId);
	struct gpu_draw_vbo_cmd_info drawVboCmdInfo = {0};
	memset((void*)&drawVboCmdInfo, 0, sizeof(struct gpu_draw_vbo_cmd_info));
	struct gpu_draw_vbo_info drawVboInfo = {0};
	memset((void*)&drawVboInfo, 0, sizeof(struct gpu_draw_vbo_info));
	drawVboInfo.start = 0x00;
	drawVboInfo.count = 0x03;
	drawVboInfo.mode = GPU_PRIMITIVE_TRIANGLES;
	drawVboInfo.instance_count = 0x01;
	drawVboInfo.index_size = 0x00;
	drawVboInfo.max_index = drawVboInfo.count-0x01;
	drawVboCmdInfo.header.commandType = GPU_CMD_TYPE_DRAW_VBO;
	drawVboCmdInfo.pDrawVboInfo = &drawVboInfo;
	gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&drawVboCmdInfo);
	if (gpu_cmd_context_submit(gpuId, contextId, cmdContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
		virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
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
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		gpu_cmd_context_deinit(gpuId, cmdContextId);
		return -1;
	}
	virtualFree((uint64_t)pVertexBuffer, vertexBufferSize);
	gpu_resource_delete(gpuId, vertexBufferResourceId);
	gpu_cmd_context_deinit(gpuId, cmdContextId);
	while (1){};
	return 0;
}
