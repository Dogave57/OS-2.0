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
	struct gpu_vertex_element vertexElementList[1] = {0};
	memset((void*)vertexElementList, 0, sizeof(struct gpu_vertex_element));
	vertexElementList[0].src_offset = 0x00;
	vertexElementList[0].vertex_buffer_index = 0x00;
	vertexElementList[0].src_format = GPU_FORMAT_R32G32_FLOAT;
	createVertexElementListObjectInfo.header.objectType = GPU_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
	createVertexElementListObjectInfo.surfaceObjectId = surfaceObjectId;
	createVertexElementListObjectInfo.pVertexElementList = (struct gpu_vertex_element*)vertexElementList;
	createVertexElementListObjectInfo.vertexElementCount = 0x01;
	if (gpu_object_create(pMonitorDesc->gpuId, contextId, (struct gpu_create_object_info*)&createVertexElementListObjectInfo, &vertexElementListObjectId)!=0){
		printf("failed to create GPU host controller vertex element list object\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	static const unsigned char vertexShaderCode[]="VERT\n"
		"DCL OUT[0], POSITION\n"
		"DCL OUT[1], COLOR\n"
		"DCL CONST[0..2]\n"
		"DCL TEMP[0]\n"
		"DCL IN[0]\n"
		"MOV TEMP[0], IN[0]\n"
		"MUL TEMP[0].xy, TEMP[0], CONST[2]\n"
		"ADD TEMP[0].xy, TEMP[0], CONST[1]\n"
		"MOV OUT[0], TEMP[0]\n"
		"MOV OUT[1], CONST[0]\n"
		"END\n";
	static const unsigned char fragmentShaderCode[]="FRAG\n"
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
	uint64_t vertexBufferResourceId = 0x00;
	struct gpu_create_resource_info createResourceInfo = {0};
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32G32B32A32_FLOAT;
	createResourceInfo.resourceInfo.normal.format = 0x40;
	createResourceInfo.resourceInfo.normal.width = PAGE_SIZE;
	createResourceInfo.resourceInfo.normal.height = 0x01;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.arraySize = 0x00;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_VERTEX_BUFFER;
	createResourceInfo.resourceInfo.normal.flags = 0x00;
	if (gpu_resource_create(pMonitorDesc->gpuId, createResourceInfo, &vertexBufferResourceId)!=0){
		printf("failed to create GPU host controller vertex buffer resource\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return-1;
	}
	if (gpu_context_attach_resource(pMonitorDesc->gpuId, contextId, vertexBufferResourceId)!=0){
		printf("failed to attach GPU host controller vertex buffer resource to framebuffer context\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	uint64_t indexBufferResourceId = 0x00;
	memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
	createResourceInfo.resourceInfo.contextId = contextId;
	createResourceInfo.resourceInfo.normal.format = 0x40;
	createResourceInfo.resourceInfo.normal.width = PAGE_SIZE;
	createResourceInfo.resourceInfo.normal.height = 0x01;
	createResourceInfo.resourceInfo.normal.depth = 0x01;
	createResourceInfo.resourceInfo.normal.arraySize = 0x01;
	createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
	createResourceInfo.resourceInfo.normal.bind = GPU_BIND_INDEX_BUFFER;
	createResourceInfo.resourceInfo.normal.flags = 0x00;
	if (gpu_resource_create(pMonitorDesc->gpuId, createResourceInfo, &indexBufferResourceId)!=0){
		printf("failed to create GPU host controller index buffer resource\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	if (gpu_context_attach_resource(pMonitorDesc->gpuId, contextId, indexBufferResourceId)!=0){
		printf("failed to attach GPU host controller index buffer resource to framebuffer context\r\n");
		subsystem_deinit(pFontDriverSubsystemDesc);
		subsystem_deinit(pFontSubsystemDesc);
		return -1;
	}
	textSubsystemInfo.pMonitorDesc = pMonitorDesc;
	textSubsystemInfo.pDriverDesc = pDriverDesc;
	textSubsystemInfo.pGpuDesc = pGpuDesc;
	textSubsystemInfo.accelerationInfo.contextId = contextId;
	textSubsystemInfo.accelerationInfo.surfaceObjectId = surfaceObjectId;
	textSubsystemInfo.accelerationInfo.rasterizerStateObjectId = rasterizerStateObjectId;
	textSubsystemInfo.accelerationInfo.dsaStateObjectId = dsaStateObjectId;
	textSubsystemInfo.accelerationInfo.blendStateListObjectId = blendStateListObjectId;
	textSubsystemInfo.accelerationInfo.vertexElementListObjectId = vertexElementListObjectId;
	textSubsystemInfo.accelerationInfo.vertexShaderObjectId = vertexShaderObjectId;
	textSubsystemInfo.accelerationInfo.fragmentShaderObjectId = fragmentShaderObjectId;
	textSubsystemInfo.accelerationInfo.vertexBufferResourceId = vertexBufferResourceId;
	textSubsystemInfo.accelerationInfo.indexBufferResourceId = indexBufferResourceId;
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
KAPI int text_subsystem_font_driver_register(struct text_subsystem_font_driver_info driverInfo, uint64_t* pFontDriverId){
	if (!pFontDriverId||!textSubsystemInfo.fontDriverSubsystemInfo.pSubsystemDesc)
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
	pFontDriverDesc->fontDriverId = fontDriverId;
	pFontDriverDesc->driverInfo = driverInfo;
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
	if (pFontDriverDesc->driverInfo.vtable.fontGlyphGetId(pFontDesc, 'V', &glyphId)!=0){
		printf("failed to get glyph ID via font driver\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	printf("glyph ID: %d\r\n", glyphId);
	struct text_subsystem_glyph_vertex* pGlyphVertexBuffer = (struct text_subsystem_glyph_vertex*)0x00;
	uint64_t glyphVertexBufferSize = 0x00;
	uint64_t glyphVertexEntryCount = 0x00;
	if (virtualAllocPage((uint64_t*)&pGlyphVertexBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for GPU host controller glyph vertex buffer\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	uint16_t* pGlyphIndexBuffer = (uint16_t*)0x00;
	uint64_t glyphIndexBufferSize = 0x00;
	uint64_t glyphIndexEntryCount = 0x00;
	if (virtualAllocPage((uint64_t*)&pGlyphIndexBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for GPU host controller glyph index buffer\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pFontDriverDesc->driverInfo.vtable.fontGlyphTesselate(pFontDesc, glyphId, pGlyphVertexBuffer, pGlyphIndexBuffer, &glyphVertexBufferSize, &glyphIndexBufferSize)!=0){
		printf("failed to tesselate font glyph via font driver\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	glyphVertexEntryCount = glyphVertexBufferSize/sizeof(struct text_subsystem_glyph_vertex);
	glyphIndexEntryCount = glyphIndexBufferSize/sizeof(uint16_t);
	virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
	virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
	while (1){};
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.rasterizerStateObjectId)!=0){
		printf("failed to bind GPU host controller rasterizer state object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.dsaStateObjectId)!=0){
		printf("failed to bind GPU Host controller DSA state object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.blendStateListObjectId)!=0){
		printf("failed to bind GPU host controller blend state list object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexElementListObjectId)!=0){
		printf("failed to bind GPU host controller vertex element list object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexShaderObjectId)!=0){
		printf("failed to bind GPU host controller vertex shader object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_bind(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.fragmentShaderObjectId)!=0){
		printf("failed to bind GPU host controller fragment shader object\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_resource_attach_backing(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexBufferResourceId, (unsigned char*)pGlyphVertexBuffer, PAGE_SIZE)!=0){
		printf("failed to attach GPU host controller vertex buffer resource physical pages to GPU host controller vertex buffer resource\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_resource_attach_backing(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.indexBufferResourceId, (unsigned char*)pGlyphIndexBuffer, PAGE_SIZE)!=0){
		printf("failed to attach GPU host controller index buffer resource physical pages to GPU host controller index buffer resource\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_transfer_to_device_info transferToDeviceInfo = {0};
	memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
	transferToDeviceInfo.boxRect.width = glyphVertexBufferSize;
	transferToDeviceInfo.boxRect.height = 0x01;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.vertexBufferResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller vertex buffer resource data to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	transferToDeviceInfo.boxRect.width = glyphIndexBufferSize;
	transferToDeviceInfo.boxRect.height = 0x01;
	transferToDeviceInfo.boxRect.depth = 0x01;
	if (gpu_transfer_to_device(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.indexBufferResourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer GPU host controller index buffer resource data to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct fvec4_32 constantBuffer[3] = {0};
	struct fvec4_32* pGlyphColor = (struct fvec4_32*)constantBuffer;
	pGlyphColor->x = 1.0f;
	pGlyphColor->y = 0.0f;
	pGlyphColor->z = 1.0f;
	pGlyphColor->w = 1.0f;
	struct fvec4_32* pTranslationVector = pGlyphColor+0x01;
	pTranslationVector->x = 0.5f;
	pTranslationVector->y = 0.5f;
	pTranslationVector->z = 0.0f;
	pTranslationVector->w = 1.0f;
	struct fvec4_32* pScaleVector = pGlyphColor+0x02;
	pScaleVector->x = 0.05f;
	pScaleVector->y = 0.1f;
	pScaleVector->z = 0.0f;
	pScaleVector->w = 1.0f;
	gpu_cmd_context_reset(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId);
	struct gpu_set_vertex_buffer_list_cmd_info setVertexBufferListCmdInfo = {0};
	memset((void*)&setVertexBufferListCmdInfo, 0, sizeof(struct gpu_set_vertex_buffer_list_cmd_info));
	struct gpu_vertex_buffer vertexBufferList[1] = {0};
	memset((void*)vertexBufferList, 0, sizeof(struct gpu_vertex_buffer));
	vertexBufferList[0].stride = sizeof(struct text_subsystem_glyph_vertex);
	vertexBufferList[0].resource_id = textSubsystemInfo.accelerationInfo.vertexBufferResourceId;
	setVertexBufferListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VERTEX_BUFFER_LIST;
	setVertexBufferListCmdInfo.vertexBufferCount = 0x01;
	setVertexBufferListCmdInfo.pVertexBufferList = (struct gpu_vertex_buffer*)vertexBufferList;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setVertexBufferListCmdInfo);
	struct gpu_set_index_buffer_cmd_info setIndexBufferCmdInfo = {0};
	memset((void*)&setIndexBufferCmdInfo, 0, sizeof(struct gpu_set_index_buffer_cmd_info));
	setIndexBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_INDEX_BUFFER;
	setIndexBufferCmdInfo.resourceId = textSubsystemInfo.accelerationInfo.indexBufferResourceId;
	setIndexBufferCmdInfo.length = sizeof(uint16_t);
	setIndexBufferCmdInfo.offset = 0x00;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setIndexBufferCmdInfo);
	struct gpu_set_constant_buffer_cmd_info setConstantBufferCmdInfo = {0};
	memset((void*)&setConstantBufferCmdInfo, 0, sizeof(struct gpu_set_constant_buffer_cmd_info));
	setConstantBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_CONSTANT_BUFFER;
	setConstantBufferCmdInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
	setConstantBufferCmdInfo.index = 0x00;
	setConstantBufferCmdInfo.pBuffer = (unsigned char*)constantBuffer;
	setConstantBufferCmdInfo.bufferSize = sizeof(struct fvec4_32)*0x03;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&setConstantBufferCmdInfo);
	struct gpu_draw_vbo_cmd_info drawVboCmdInfo = {0};
	memset((void*)&drawVboCmdInfo, 0, sizeof(struct gpu_draw_vbo_cmd_info));
	struct gpu_draw_vbo_info drawVboInfo = {0};
	memset((void*)&drawVboInfo, 0, sizeof(struct gpu_draw_vbo_info));
	drawVboInfo.start = 0x00;
	drawVboInfo.count = glyphIndexEntryCount;
	drawVboInfo.mode = GPU_PRIMITIVE_TRIANGLES;
	drawVboInfo.instance_count = 0x01;
	drawVboInfo.index_size = glyphIndexEntryCount;
	drawVboInfo.max_index = drawVboInfo.count-1;
	drawVboCmdInfo.header.commandType = GPU_CMD_TYPE_DRAW_VBO;
	drawVboCmdInfo.pDrawVboInfo = &drawVboInfo;
	gpu_cmd_context_push_cmd(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.commandContextId, (struct gpu_cmd_info_header*)&drawVboCmdInfo);
	if (gpu_cmd_context_submit(textSubsystemInfo.pMonitorDesc->gpuId, textSubsystemInfo.accelerationInfo.contextId, textSubsystemInfo.accelerationInfo.commandContextId)!=0){
		printf("failed to submit GPU host controller command list to GPU host controller\r\n");
		subsystem_free_entry(textSubsystemInfo.fontSubsystemInfo.pSubsystemDesc, fontId);
		virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
		virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pGlyphVertexBuffer, 0);
	virtualFreePage((uint64_t)pGlyphIndexBuffer, 0);
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
	while (1){};
	*pFontId = fontId;
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
