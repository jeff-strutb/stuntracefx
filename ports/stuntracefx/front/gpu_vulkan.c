/*
 * gpu_vulkan.c: the cartridge's GPU, Vulkan (Windows, Linux; macOS through
 * MoltenVK, a check): the full window's spans filled by compute kernels,
 * as gpu_metal.m does on Apple's: the polygons' rows made a thread a row
 * (gpu_vk/rows.comp, fw_poly_row's steps), then each row's spans filled in
 * their order (gpu_vk/fill.comp, fw_span_fill_cpu's rules), so the picture
 * is the CPU's to the pixel. The picture is in memory the CPU and the GPU
 * share (fw_texels_alloc). The Vulkan library is loaded when the game
 * starts: with none, or no GPU that has 64-bit integers in its shaders,
 * the CPU fills (and says so). FW_GPUCHECK=1 (a check): each fill drawn by
 * the CPU too and compared, the count of pixels apart written at exit.
 */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "fullwindow.h"
#include "gpu_bin.h"
#include "gpu_vk_spv.h"
#include "scene.h"

/* ---- the library and its calls ---- */
static PFN_vkGetInstanceProcAddr gipa;
#define VK_FUNCS(X) \
    X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties)
#define VK_IFUNCS(X) \
    X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceFeatures) \
    X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkCreateDevice) \
    X(vkGetDeviceProcAddr) X(vkEnumerateDeviceExtensionProperties)
#define VK_DFUNCS(X) \
    X(vkGetDeviceQueue) X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) \
    X(vkAllocateMemory) X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkCreateShaderModule) \
    X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateComputePipelines) \
    X(vkCreateDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateCommandPool) \
    X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdBindPipeline) \
    X(vkCmdBindDescriptorSets) X(vkCmdPushConstants) X(vkCmdDispatch) X(vkCmdPipelineBarrier) \
    X(vkQueueSubmit) X(vkCreateFence) X(vkWaitForFences) X(vkResetFences) X(vkResetCommandBuffer) \
    X(vkDeviceWaitIdle) X(vkCreateImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView) \
    X(vkCreateRenderPass) X(vkCreateFramebuffer) X(vkCreateGraphicsPipelines) X(vkCmdBeginRenderPass) \
    X(vkCmdEndRenderPass) X(vkCmdDraw) X(vkCmdCopyBufferToImage) X(vkCmdCopyImageToBuffer) X(vkCmdSetViewport) \
    X(vkCmdSetScissor) X(vkDestroyImage) X(vkDestroyImageView) X(vkDestroyFramebuffer) X(vkCmdClearColorImage) \
    X(vkCmdCopyBuffer)
#define DECL(f) static PFN_##f p##f;
VK_FUNCS(DECL) VK_IFUNCS(DECL) VK_DFUNCS(DECL)
#undef DECL

static int lib_load(void)
{
#if defined(_WIN32)
    HMODULE m = LoadLibraryA("vulkan-1.dll");
    if (m)
        gipa = (PFN_vkGetInstanceProcAddr)(void (*)(void))GetProcAddress(m, "vkGetInstanceProcAddr");
#else
    static const char *const names[] = {"libvulkan.so.1", "libvulkan.1.dylib", "libvulkan.dylib",
                                        "/opt/homebrew/lib/libvulkan.1.dylib", "/usr/local/lib/libvulkan.1.dylib"};
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0] && !gipa; i++) {
        void *m = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (m)
            gipa = (PFN_vkGetInstanceProcAddr)dlsym(m, "vkGetInstanceProcAddr");
    }
#endif
    if (!gipa)
        return 0;
#define LOAD(f) if (!(p##f = (PFN_##f)gipa(NULL, #f))) return 0;
    VK_FUNCS(LOAD)
#undef LOAD
    return 1;
}

/* ---- the device ---- */
static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static uint32_t qfam;
static VkPhysicalDeviceMemoryProperties memp;
static VkDescriptorSetLayout dsl;
static VkPipelineLayout pl;
static VkPipeline pipe_rows, pipe_fill, pipe_water, pipe_prims, pipe_pack, pipe_haze, pipe_noise;
/* the models (scene.h): the render pass over the picture (R8 colour
   indices) and each pixel's face (R32), their pipelines (triangles,
   lines), the images and the framebuffer at the picture's size */
static VkRenderPass rpass;
static VkPipeline pipe_tri, pipe_line, pipe_quad;
static VkImage img_pic, img_id;
static VkDeviceMemory mem_pic, mem_id;
static VkImageView view_pic, view_id;
static VkFramebuffer fbuf;
static int img_w, img_h, id_clear;
static long faces_drawn;
static VkDescriptorPool dpool;
static VkDescriptorSet dset;
static VkCommandPool cpool;
static VkCommandBuffer cb;
static VkFence fence;

/* a buffer the CPU and GPU share, mapped */
typedef struct {
    VkBuffer b;
    VkDeviceMemory m;
    size_t len;
    void *at;
} Buf;
enum { B_OUT, B_SPANS, B_ROW_AT, B_ROW_SPANS, B_BANKS, B_SLOT, B_POLYS, B_EDGES, B_ROW_POLY, B_OUT32, B_JOB, B_GAP,
       B_VERTS, B_LPOINTS, B_LSLOTS, B_LFACES, B_ID, B_FSLOTS, B_FPOINTS, B_QUADS, B_PRIMS, B_SNAP, B_BITS, B_N };
static const uint32_t binding_of[B_N] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
/* the stages that see the descriptors and the push constants: the fill's
   kernels and the models' shaders */
#define STAGES (VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
static Buf bufs[B_N];
static int dset_stale = 1;

static int mem_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    uint32_t i;
    for (i = 0; i < memp.memoryTypeCount; i++)
        if ((bits >> i & 1) && (memp.memoryTypes[i].propertyFlags & want) == want)
            return (int)i;
    return -1;
}

static int buf_grow(int which, size_t len)
{
    Buf *b = &bufs[which];
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VkMemoryRequirements req;
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    int t;
    if (len < 16)
        len = 16;
    if (b->b && b->len >= len)
        return 1;
    if (b->b) {
        pvkDeviceWaitIdle(dev);
        pvkDestroyBuffer(dev, b->b, NULL);
        pvkFreeMemory(dev, b->m, NULL);
        memset(b, 0, sizeof *b);
    }
    len += len / 2;                     /* room to grow */
    bi.size = len;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (pvkCreateBuffer(dev, &bi, NULL, &b->b) != VK_SUCCESS)
        return 0;
    pvkGetBufferMemoryRequirements(dev, b->b, &req);
    t = mem_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                         VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (t < 0)
        t = mem_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (t < 0)
        return 0;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = (uint32_t)t;
    if (pvkAllocateMemory(dev, &ai, NULL, &b->m) != VK_SUCCESS || pvkBindBufferMemory(dev, b->b, b->m, 0) != VK_SUCCESS ||
        pvkMapMemory(dev, b->m, 0, VK_WHOLE_SIZE, 0, &b->at) != VK_SUCCESS)
        return 0;
    b->len = len;
    dset_stale = 1;
    return 1;
}

static void dset_update(void)
{
    VkDescriptorBufferInfo bi[B_N];
    VkWriteDescriptorSet wr[B_N];
    int i;
    for (i = 0; i < B_N; i++) {
        bi[i].buffer = bufs[i].b;
        bi[i].offset = 0;
        bi[i].range = VK_WHOLE_SIZE;
        memset(&wr[i], 0, sizeof wr[i]);
        wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr[i].dstSet = dset;
        wr[i].dstBinding = binding_of[i];
        wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[i].pBufferInfo = &bi[i];
    }
    pvkUpdateDescriptorSets(dev, B_N, wr, 0, NULL);
    dset_stale = 0;
}

static VkPipeline pipeline(const uint32_t *code, size_t len)
{
    VkShaderModuleCreateInfo si = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    VkShaderModule m;
    VkPipeline p = VK_NULL_HANDLE;
    si.codeSize = len;
    si.pCode = code;
    if (pvkCreateShaderModule(dev, &si, NULL, &m) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ci.stage.module = m;
    ci.stage.pName = "main";
    ci.layout = pl;
    if (pvkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, NULL, &p) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return p;
}

/* the render pass and the models' pipelines */
static VkShaderModule shader(const uint32_t *code, size_t len)
{
    VkShaderModuleCreateInfo si = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModule m = VK_NULL_HANDLE;
    si.codeSize = len;
    si.pCode = code;
    if (pvkCreateShaderModule(dev, &si, NULL, &m) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return m;
}

static int models_open(void)
{
    VkAttachmentDescription at[2];
    VkAttachmentReference ref[2] = {{0, VK_IMAGE_LAYOUT_GENERAL}, {1, VK_IMAGE_LAYOUT_GENERAL}};
    VkSubpassDescription sub = {0};
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    VkPipelineShaderStageCreateInfo st[2];
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState cba[2];
    VkPipelineColorBlendStateCreateInfo cb_ = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo gi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    int i;

    memset(at, 0, sizeof at);
    for (i = 0; i < 2; i++) {
        at[i].format = i ? VK_FORMAT_R32_UINT : VK_FORMAT_R8_UINT;
        at[i].samples = VK_SAMPLE_COUNT_1_BIT;
        at[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        at[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        at[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        at[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        at[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        at[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 2;
    sub.pColorAttachments = ref;
    rpi.attachmentCount = 2;
    rpi.pAttachments = at;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sub;
    if (pvkCreateRenderPass(dev, &rpi, NULL, &rpass) != VK_SUCCESS)
        return 0;
    memset(st, 0, sizeof st);
    for (i = 0; i < 2; i++) {
        st[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
        st[i].module = i ? shader(k_model_frag, sizeof k_model_frag) : shader(k_model_vert, sizeof k_model_vert);
        st[i].pName = "main";
        if (!st[i].module)
            return 0;
    }
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    memset(cba, 0, sizeof cba);
    cba[0].colorWriteMask = cba[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    cb_.attachmentCount = 2;
    cb_.pAttachments = cba;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;
    gi.stageCount = 2;
    gi.pStages = st;
    gi.pVertexInputState = &vi;
    gi.pInputAssemblyState = &ia;
    gi.pViewportState = &vp;
    gi.pRasterizationState = &rs;
    gi.pMultisampleState = &ms;
    gi.pColorBlendState = &cb_;
    gi.pDynamicState = &ds;
    gi.layout = pl;
    gi.renderPass = rpass;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (pvkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gi, NULL, &pipe_tri) != VK_SUCCESS)
        return 0;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    if (pvkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gi, NULL, &pipe_line) != VK_SUCCESS)
        return 0;
    /* the screen quads' (fullwindow.h FW_QUAD) */
    st[0].module = shader(k_quad_vert, sizeof k_quad_vert);
    st[1].module = shader(k_quad_frag, sizeof k_quad_frag);
    if (!st[0].module || !st[1].module)
        return 0;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (pvkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gi, NULL, &pipe_quad) != VK_SUCCESS)
        return 0;
    return 1;
}

static int has_ext(VkExtensionProperties *e, uint32_t n, const char *name)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name))
            return 1;
    return 0;
}

static int device_open(void)
{
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkPhysicalDevice list[16];
    uint32_t n = 16, i, j, ne = 0;
    const char *iext[1];
    VkExtensionProperties *exts;
    int best = -1, best_score = -1;

    app.pApplicationName = "Stunt Race FX";
    app.apiVersion = VK_API_VERSION_1_1;
    ii.pApplicationInfo = &app;
    pvkEnumerateInstanceExtensionProperties(NULL, &ne, NULL);
    exts = calloc(ne ? ne : 1, sizeof *exts);
    if (!exts)
        return 0;
    pvkEnumerateInstanceExtensionProperties(NULL, &ne, exts);
    if (has_ext(exts, ne, "VK_KHR_portability_enumeration")) {     /* MoltenVK's */
        iext[0] = "VK_KHR_portability_enumeration";
        ii.enabledExtensionCount = 1;
        ii.ppEnabledExtensionNames = iext;
        ii.flags = 0x00000001;          /* VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR */
    }
    free(exts);
    if (pvkCreateInstance(&ii, NULL, &inst) != VK_SUCCESS)
        return 0;
#define LOAD(f) if (!(p##f = (PFN_##f)gipa(inst, #f))) return 0;
    VK_IFUNCS(LOAD)
#undef LOAD
    if (pvkEnumeratePhysicalDevices(inst, &n, list) < 0 || !n)
        return 0;
    for (i = 0; i < n; i++) {           /* a GPU with 64-bit integers and a compute queue; discrete first */
        VkPhysicalDeviceFeatures f;
        VkPhysicalDeviceProperties p;
        VkQueueFamilyProperties q[16];
        uint32_t nq = 16;
        int score;
        pvkGetPhysicalDeviceFeatures(list[i], &f);
        pvkGetPhysicalDeviceProperties(list[i], &p);
        if (!f.shaderInt64)
            continue;
        pvkGetPhysicalDeviceQueueFamilyProperties(list[i], &nq, q);
        for (j = 0; j < nq; j++)
            if (q[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
                break;
        if (j == nq)
            continue;
        score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
                : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
        if (score > best_score) {
            best = (int)i;
            best_score = score;
            qfam = j;
        }
    }
    if (best < 0)
        return 0;
    phys = list[best];
    {
        VkPhysicalDeviceProperties p;
        pvkGetPhysicalDeviceProperties(phys, &p);
        fprintf(stderr, "stuntracefx: the full window on %s (Vulkan)\n", p.deviceName);
    }
    pvkGetPhysicalDeviceMemoryProperties(phys, &memp);
    {
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        VkPhysicalDeviceFeatures want = {0};
        const char *dext[1];
        uint32_t nd = 0;
        qi.queueFamilyIndex = qfam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &prio;
        want.shaderInt64 = VK_TRUE;
        di.queueCreateInfoCount = 1;
        di.pQueueCreateInfos = &qi;
        di.pEnabledFeatures = &want;
        pvkEnumerateDeviceExtensionProperties(phys, NULL, &nd, NULL);
        if (nd && (exts = calloc(nd, sizeof *exts)) != NULL) {
            pvkEnumerateDeviceExtensionProperties(phys, NULL, &nd, exts);
            if (has_ext(exts, nd, "VK_KHR_portability_subset")) {   /* MoltenVK's, which must be named */
                dext[0] = "VK_KHR_portability_subset";
                di.enabledExtensionCount = 1;
                di.ppEnabledExtensionNames = dext;
            }
            free(exts);
        }
        if (pvkCreateDevice(phys, &di, NULL, &dev) != VK_SUCCESS)
            return 0;
    }
#define LOAD(f) if (!(p##f = (PFN_##f)pvkGetDeviceProcAddr(dev, #f))) return 0;
    VK_DFUNCS(LOAD)
#undef LOAD
    pvkGetDeviceQueue(dev, qfam, 0, &queue);
    {
        VkDescriptorSetLayoutBinding b[B_N];
        VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        VkPushConstantRange pr = {STAGES, 0, 48};   /* w, h, 0, 0, the water's (fill.comp); the view, the water's (model.vert, quad.frag) */
        VkPipelineLayoutCreateInfo pi = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, B_N};
        VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        for (i = 0; i < B_N; i++) {
            memset(&b[i], 0, sizeof b[i]);
            b[i].binding = binding_of[i];
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            b[i].descriptorCount = 1;
            b[i].stageFlags = STAGES;
        }
        li.bindingCount = B_N;
        li.pBindings = b;
        if (pvkCreateDescriptorSetLayout(dev, &li, NULL, &dsl) != VK_SUCCESS)
            return 0;
        pi.setLayoutCount = 1;
        pi.pSetLayouts = &dsl;
        pi.pushConstantRangeCount = 1;
        pi.pPushConstantRanges = &pr;
        if (pvkCreatePipelineLayout(dev, &pi, NULL, &pl) != VK_SUCCESS)
            return 0;
        dpi.maxSets = 1;
        dpi.poolSizeCount = 1;
        dpi.pPoolSizes = &ps;
        if (pvkCreateDescriptorPool(dev, &dpi, NULL, &dpool) != VK_SUCCESS)
            return 0;
        dai.descriptorPool = dpool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &dsl;
        if (pvkAllocateDescriptorSets(dev, &dai, &dset) != VK_SUCCESS)
            return 0;
        cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpi.queueFamilyIndex = qfam;
        if (pvkCreateCommandPool(dev, &cpi, NULL, &cpool) != VK_SUCCESS)
            return 0;
        cai.commandPool = cpool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        if (pvkAllocateCommandBuffers(dev, &cai, &cb) != VK_SUCCESS || pvkCreateFence(dev, &fi, NULL, &fence) != VK_SUCCESS)
            return 0;
    }
    pipe_rows = pipeline(k_rows_comp, sizeof k_rows_comp);
    pipe_fill = pipeline(k_fill_comp, sizeof k_fill_comp);
    pipe_water = pipeline(k_water_comp, sizeof k_water_comp);
    pipe_prims = pipeline(k_prims_comp, sizeof k_prims_comp);
    pipe_pack = pipeline(k_pack_comp, sizeof k_pack_comp);
    pipe_haze = pipeline(k_haze_comp, sizeof k_haze_comp);
    pipe_noise = pipeline(k_noise_comp, sizeof k_noise_comp);
    if (!pipe_rows || !pipe_fill || !pipe_water || !pipe_prims || !pipe_pack || !pipe_haze || !pipe_noise || !models_open())
        return 0;
    for (i = 0; i < B_N; i++)
        if (!buf_grow((int)i, 16))
            return 0;
    return 1;
}

/* ---- the banks the texels come from ---- */
#define SLOTS 128
static uint8_t bank_ok[256];            /* a bank's bytes in the banks' buffer */
static uint32_t bank_ver[256];        /* the version each bank's copy is of (fw_bank_version) */
/* a bank's copy as it is now: ROM's once, the RAM's and the panels' made again */
static int bank_fresh(int b) { return bank_ok[b] && b != 0x70 && b != 0x71 && bank_ver[b] == fw_bank_version((uint8_t)b); }
static int32_t bank_slot[256];
static int banks_used;

static int bank_put(uint8_t bank)
{
    uint8_t *at;
    unsigned i;
    if (bank_slot[bank] < 0) {
        if (banks_used == SLOTS)
            return 0;
        bank_slot[bank] = banks_used++;
    }
    at = (uint8_t *)bufs[B_BANKS].at + (size_t)bank_slot[bank] * 65536;
    for (i = 0; i < 65536; i++)
        at[i] = fw_texel(bank, (uint16_t)i);
    bank_ok[bank] = 1;
    bank_ver[bank] = fw_bank_version(bank);
    return 1;
}

/* ---- the fill ---- */
static GpuCheck check;

static void check_report(void) { gpu_bin_check_report("Vulkan", &check, faces_drawn); }

static uint8_t *texels_alloc(size_t len)
{
    if (!buf_grow(B_OUT, len))
        return NULL;
    return (uint8_t *)bufs[B_OUT].at;
}

/* spans (none a GPU face or the reference's alone) into the shared
   picture by the compute fill; 0 when it could not (the caller's CPU fill) */
static int fill_spans(int w, int h, const FwSpan *sp, size_t n)
{
    size_t i;
    GpuBinSizes z;
    uint32_t pc[8] = {0};
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    int b;

    if (!n)
        return 1;
    gpu_bin_sizes(sp, n, &z);
    if (!buf_grow(B_ROW_AT, ((size_t)h + 1) * 4) || !buf_grow(B_ROW_SPANS, z.total * 4) ||
        !buf_grow(B_SPANS, z.total * sizeof *sp) || !buf_grow(B_ROW_POLY, z.rows * 8) ||
        !buf_grow(B_POLYS, z.polys * sizeof(FwPoly)) || !buf_grow(B_EDGES, z.edges * sizeof(FwEdge)) ||
        !gpu_bin_fill(sp, n, h, (FwSpan *)bufs[B_SPANS].at, (uint32_t *)bufs[B_ROW_AT].at,
                      (uint32_t *)bufs[B_ROW_SPANS].at, (uint32_t *)bufs[B_ROW_POLY].at)) {
        return 0;
    }
    if (z.polys)
        memcpy(bufs[B_POLYS].at, fw_polys, z.polys * sizeof(FwPoly));
    if (z.edges)
        memcpy(bufs[B_EDGES].at, fw_edges, z.edges * sizeof(FwEdge));
    {                                   /* ROM's banks once, RAM's ($70, $71) as they are now */
        uint8_t fresh[256] = {0};
        for (i = 0; i < n; i++) {
            if ((b = gpu_bin_bank(&sp[i])) < 0 || fresh[b] || bank_fresh(b))
                continue;
            if (!bank_put((uint8_t)b))
                return 0;
            fresh[b] = 1;
        }
        /* the reciprocals a job's textured rows are stepped by (rows.comp) */
        if (z.rows && !bank_fresh(SC_TABLE_BANK_RECIP) && !bank_put(SC_TABLE_BANK_RECIP))
            return 0;
    }
    memcpy(bufs[B_SLOT].at, bank_slot, sizeof bank_slot);
    if (dset_stale)
        dset_update();
    pvkResetCommandBuffer(cb, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    pvkBeginCommandBuffer(cb, &bi);
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
    if (z.rows) {                       /* the polygons' rows, then the fill */
        pc[0] = (uint32_t)w;
        pc[1] = (uint32_t)z.rows;
        pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_rows);
        pvkCmdPushConstants(cb, pl, STAGES, 0, 8, pc);
        pvkCmdDispatch(cb, (uint32_t)((z.rows + 63) / 64), 1, 1);
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb,
                              0, NULL, 0, NULL);
    }
    pc[0] = (uint32_t)w;
    pc[1] = (uint32_t)h;
    pc[2] = pc[3] = 0;
    memcpy(&pc[4], fw_water, 16);
    pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_fill);
    pvkCmdPushConstants(cb, pl, STAGES, 0, 32, pc);
    pvkCmdDispatch(cb, (uint32_t)(((w + 31) / 32 + 7) / 8), (uint32_t)((h + 7) / 8), 1);
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
                          NULL);
    pvkEndCommandBuffer(cb);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    pvkResetFences(dev, 1, &fence);
    if (pvkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS ||
        pvkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull) != VK_SUCCESS)
        return 0;
    return 1;
}

/* one command buffer recorded by fn, run, waited for */
static int run_commands(void (*fn)(void *), void *arg)
{
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    pvkResetCommandBuffer(cb, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    pvkBeginCommandBuffer(cb, &bi);
    fn(arg);
    pvkEndCommandBuffer(cb);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    pvkResetFences(dev, 1, &fence);
    return pvkQueueSubmit(queue, 1, &si, fence) == VK_SUCCESS &&
           pvkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull) == VK_SUCCESS;
}

static void barrier(VkPipelineStageFlags from, VkAccessFlags src, VkPipelineStageFlags to, VkAccessFlags dst)
{
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = src;
    mb.dstAccessMask = dst;
    pvkCmdPipelineBarrier(cb, from, to, 0, 1, &mb, 0, NULL, 0, NULL);
}

static void to_general(void *arg)
{
    VkImageMemoryBarrier ib[2];
    int i;
    (void)arg;
    memset(ib, 0, sizeof ib);
    for (i = 0; i < 2; i++) {
        ib[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ib[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ib[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ib[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        ib[i].srcQueueFamilyIndex = ib[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib[i].image = i ? img_id : img_pic;
        ib[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ib[i].subresourceRange.levelCount = 1;
        ib[i].subresourceRange.layerCount = 1;
    }
    pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, ib);
}

/* the render target at the picture's size: the colour indices and the faces */
static int images_at(int w, int h)
{
    int i;
    if (img_pic && img_w == w && img_h == h)
        return 1;
    pvkDeviceWaitIdle(dev);
    if (img_pic) {
        pvkDestroyFramebuffer(dev, fbuf, NULL);
        pvkDestroyImageView(dev, view_pic, NULL);
        pvkDestroyImageView(dev, view_id, NULL);
        pvkDestroyImage(dev, img_pic, NULL);
        pvkDestroyImage(dev, img_id, NULL);
        pvkFreeMemory(dev, mem_pic, NULL);
        pvkFreeMemory(dev, mem_id, NULL);
        img_pic = img_id = VK_NULL_HANDLE;
    }
    for (i = 0; i < 2; i++) {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        VkMemoryRequirements req;
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkImage *im = i ? &img_id : &img_pic;
        VkDeviceMemory *mem = i ? &mem_id : &mem_pic;
        int t;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = i ? VK_FORMAT_R32_UINT : VK_FORMAT_R8_UINT;
        ii.extent.width = (uint32_t)w;
        ii.extent.height = (uint32_t)h;
        ii.extent.depth = 1;
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (pvkCreateImage(dev, &ii, NULL, im) != VK_SUCCESS)
            return 0;
        pvkGetImageMemoryRequirements(dev, *im, &req);
        if ((t = mem_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) < 0)
            return 0;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = (uint32_t)t;
        if (pvkAllocateMemory(dev, &ai, NULL, mem) != VK_SUCCESS || pvkBindImageMemory(dev, *im, *mem, 0) != VK_SUCCESS)
            return 0;
        vi.image = *im;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = ii.format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (pvkCreateImageView(dev, &vi, NULL, i ? &view_id : &view_pic) != VK_SUCCESS)
            return 0;
    }
    {
        VkImageView v[2] = {view_pic, view_id};
        VkFramebufferCreateInfo fi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = rpass;
        fi.attachmentCount = 2;
        fi.pAttachments = v;
        fi.width = (uint32_t)w;
        fi.height = (uint32_t)h;
        fi.layers = 1;
        if (pvkCreateFramebuffer(dev, &fi, NULL, &fbuf) != VK_SUCCESS)
            return 0;
    }
    if (!run_commands(to_general, NULL))
        return 0;
    img_w = w;
    img_h = h;
    id_clear = 1;
    return 1;
}

/* a run of display-list faces' draw (draw_faces), recorded */
typedef struct {
    int w, h;
    size_t nr, nt, nq, nprim;
    const uint8_t *kind;
    const uint32_t *len;
    float view[12];
} FaceJob;

static void faces_record(void *arg)
{
    FaceJob *j = (FaceJob *)arg;
    VkBufferImageCopy bc = {0};
    VkRenderPassBeginInfo rb = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    VkViewport vp = {0, 0, 0, 0, 0, 1};
    VkRect2D sc = {{0, 0}, {0, 0}};
    size_t r, t0 = 0, l0 = j->nt, q0 = 0;
    bc.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bc.imageSubresource.layerCount = 1;
    bc.bufferRowLength = (uint32_t)j->w;
    bc.imageExtent.width = (uint32_t)j->w;
    bc.imageExtent.height = (uint32_t)j->h;
    bc.imageExtent.depth = 1;
    if (j->nprim) {                     /* the objects' primitives into their quads (prims.comp) */
        uint32_t nq = (uint32_t)j->nq;
        pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
        pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_prims);
        pvkCmdPushConstants(cb, pl, STAGES, 0, 4, &nq);
        pvkCmdDispatch(cb, (nq + 63) / 64, 1, 1);
        barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    }
    barrier(VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    pvkCmdCopyBufferToImage(cb, bufs[B_OUT].b, img_pic, VK_IMAGE_LAYOUT_GENERAL, 1, &bc);
    if (id_clear) {
        VkClearColorValue zero = {{0}};
        VkImageSubresourceRange rr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        pvkCmdClearColorImage(cb, img_id, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &rr);
        id_clear = 0;
    }
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    rb.renderPass = rpass;
    rb.framebuffer = fbuf;
    rb.renderArea.extent.width = (uint32_t)j->w;
    rb.renderArea.extent.height = (uint32_t)j->h;
    pvkCmdBeginRenderPass(cb, &rb, VK_SUBPASS_CONTENTS_INLINE);
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1, &dset, 0, NULL);
    pvkCmdPushConstants(cb, pl, STAGES, 0, 48, j->view);
    vp.width = (float)j->w;
    vp.height = (float)j->h;
    sc.extent.width = (uint32_t)j->w;
    sc.extent.height = (uint32_t)j->h;
    pvkCmdSetViewport(cb, 0, 1, &vp);
    pvkCmdSetScissor(cb, 0, 1, &sc);
    for (r = 0; r < j->nr; r++) {
        if (j->kind[r] == 2) {
            pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_quad);
            pvkCmdDraw(cb, j->len[r], 1, (uint32_t)q0, 0);
            q0 += j->len[r];
        } else if (j->kind[r]) {
            pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_line);
            pvkCmdDraw(cb, j->len[r], 1, (uint32_t)l0, 0);
            l0 += j->len[r];
        } else if (j->len[r]) {
            pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_tri);
            pvkCmdDraw(cb, j->len[r], 1, (uint32_t)t0, 0);
            t0 += j->len[r];
        }
    }
    pvkCmdEndRenderPass(cb);
    barrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    pvkCmdCopyImageToBuffer(cb, img_pic, VK_IMAGE_LAYOUT_GENERAL, bufs[B_OUT].b, 1, &bc);
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
}

/* a run of GPU faces (spans with textured 6) drawn over the picture in
   their order (gpu_metal.m's draw_faces, the same display list) */
static int draw_faces(int w, int h, const FwSpan *sp, size_t n)
{
    size_t ns, np, nf, nt, nl, nq, i, nfs, nfp, nprim;
    const ScPrim *prims = sc_list_prims(&nprim);
    const ScSlot *sl = sc_list_slots(&ns);
    const ScPoint *pt = sc_list_points(&np);
    const ScFace *fc = sc_list_faces(&nf);
    const ScFSlot *fs = sc_list_fslots(&nfs);
    const ScFPoint *fp = sc_list_fpoints(&nfp);
    static uint8_t *kind;
    static uint32_t *len;
    static size_t cap;
    FaceJob j;
    float cx, cy;

    if (cap < n) {
        free(kind);
        free(len);
        kind = malloc(n);
        len = malloc(n * 4);
        cap = kind && len ? n : 0;
        if (!cap)
            return 0;
    }
    {
        uint8_t put[256] = {0};         /* the faces' and quads' banks in their slots, each copied once a run */
        for (i = 0; i < n; i++) {
            const ScFace *q;
            int b = -1;
            if (sp[i].textured == FW_QUAD && !(sp[i].pad1[1] & 2) && !gpu_bin_ref_only(&sp[i]))
                b = sp[i].bank;
            else if (sp[i].textured == 6 && (size_t)sp[i].x1 < nf && (q = &fc[sp[i].x1])->textured)
                b = q->bank;
            if (b < 0 || put[b] || bank_fresh(b))
                continue;
            if (!bank_put((uint8_t)b))
                return 0;
            put[b] = 1;
        }
        /* the ROM's tables the primitives are made by */
        if (nprim && ((!bank_fresh(SC_TABLE_BANK_SCALE) && !bank_put(SC_TABLE_BANK_SCALE)) ||
                      (!bank_fresh(SC_TABLE_BANK_RECIP) && !bank_put(SC_TABLE_BANK_RECIP))))
            return 0;
    }
    memcpy(bufs[B_SLOT].at, bank_slot, sizeof bank_slot);
    gpu_bin_faces(sp, n, NULL, &nt, NULL, &nl, NULL, &nq, NULL, NULL, NULL);
    if (!images_at(w, h) || !buf_grow(B_VERTS, (nt + nl) * sizeof(GpuVert)) || !buf_grow(B_LPOINTS, np * sizeof *pt) ||
        !buf_grow(B_QUADS, nq * sizeof(FwSpan)) || !buf_grow(B_PRIMS, sizeof(ScPrimView) + nprim * sizeof(ScPrim)) ||
        !buf_grow(B_LSLOTS, ns * sizeof *sl) || !buf_grow(B_LFACES, nf * sizeof *fc) ||
        !buf_grow(B_FSLOTS, nfs * sizeof *fs) || !buf_grow(B_FPOINTS, nfp * sizeof *fp))
        return 0;
    gpu_bin_faces(sp, n, (GpuVert *)bufs[B_VERTS].at, &nt, (GpuVert *)bufs[B_VERTS].at + nt, &nl, (FwSpan *)bufs[B_QUADS].at,
                  &nq, &j.nr, kind, len);
    if (np)
        memcpy(bufs[B_LPOINTS].at, pt, np * sizeof *pt);
    if (ns)
        memcpy(bufs[B_LSLOTS].at, sl, ns * sizeof *sl);
    if (nf)
        memcpy(bufs[B_LFACES].at, fc, nf * sizeof *fc);
    if (nfs)
        memcpy(bufs[B_FSLOTS].at, fs, nfs * sizeof *fs);
    if (nfp)
        memcpy(bufs[B_FPOINTS].at, fp, nfp * sizeof *fp);
    memcpy(bufs[B_PRIMS].at, sc_list_prim_view(), sizeof(ScPrimView));
    if (nprim)
        memcpy((uint8_t *)bufs[B_PRIMS].at + sizeof(ScPrimView), prims, nprim * sizeof *prims);
    if (dset_stale)
        dset_update();
    fw_view_centre(&cx, &cy);
    j.w = w;
    j.h = h;
    j.nt = nt;
    j.nq = nq;
    j.nprim = nprim;
    j.kind = kind;
    j.len = len;
    j.view[0] = (float)fw_focal_x();
    j.view[1] = (float)fw_focal_y();
    j.view[2] = cx;
    j.view[3] = cy;
    j.view[4] = (float)w;
    j.view[5] = (float)h;
    j.view[6] = j.view[7] = 0;
    memcpy(j.view + 8, fw_water, sizeof fw_water);
    if (!run_commands(faces_record, &j))
        return 0;
    faces_drawn += (long)n;
    return 1;
}

static void ids_record(void *arg)
{
    VkBufferImageCopy bc = {0};
    int *wh = (int *)arg;
    bc.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bc.imageSubresource.layerCount = 1;
    bc.bufferRowLength = (uint32_t)wh[0];
    bc.imageExtent.width = (uint32_t)wh[0];
    bc.imageExtent.height = (uint32_t)wh[1];
    bc.imageExtent.depth = 1;
    pvkCmdCopyImageToBuffer(cb, img_id, VK_IMAGE_LAYOUT_GENERAL, bufs[B_ID].b, 1, &bc);
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
}

/* each pixel's last display-list face (+ 1), for a dump */
static const uint32_t *face_ids(int w, int h)
{
    int wh[2] = {w, h};
    if (!img_id || img_w != w || img_h != h || id_clear || !buf_grow(B_ID, (size_t)w * h * 4))
        return NULL;
    if (dset_stale)
        dset_update();
    return run_commands(ids_record, wh) ? (const uint32_t *)bufs[B_ID].at : NULL;
}

/* one effect's haze (haze.comp): the picture copied as it stands, its shape's
   entries read from the copy and written */
typedef struct {
    size_t len;
    int32_t pc[10];
} HazeJob;

static void haze_record(void *arg)
{
    HazeJob *j = (HazeJob *)arg;
    VkBufferCopy c = {0, 0, j->len};
    pvkCmdCopyBuffer(cb, bufs[B_OUT].b, bufs[B_SNAP].b, 1, &c);
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
    pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_haze);
    pvkCmdPushConstants(cb, pl, STAGES, 0, sizeof j->pc, j->pc);
    pvkCmdDispatch(cb, 1, 1, 1);
    barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
}

static int haze_run(int w, int h, const FwSpan *p)
{
    HazeJob j;
    uint8_t haze_bank = 8;
    j.len = (size_t)w * h;
    if (!buf_grow(B_SNAP, j.len) || (!bank_fresh(haze_bank) && !bank_put(haze_bank)))
        return 0;
    memcpy(bufs[B_SLOT].at, bank_slot, sizeof bank_slot);
    if (dset_stale)
        dset_update();
    j.pc[0] = w;
    j.pc[1] = h;
    j.pc[2] = (int32_t)p->xl;
    j.pc[3] = p->pad0;
    j.pc[4] = p->base;
    j.pc[5] = (int32_t)p->u;
    j.pc[6] = (int32_t)p->du;
    j.pc[7] = (int32_t)p->v;
    j.pc[8] = (int32_t)p->dv;
    j.pc[9] = p->pad1[2];
    return run_commands(haze_record, &j);
}

/* a job's noise (noise.comp) */
static void noise_record(void *arg)
{
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
    pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_noise);
    pvkCmdPushConstants(cb, pl, STAGES, 0, 32, arg);
    pvkCmdDispatch(cb, 1, 1, 1);
    barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
}

static int noise_run(int w, int h, const FwSpan *p)
{
    uint32_t pc[8];
    uint64_t ramp = (uint64_t)p->u;
    if (dset_stale)
        dset_update();
    pc[0] = (uint32_t)w;
    pc[1] = (uint32_t)h;
    pc[2] = (uint32_t)p->xl;
    pc[3] = (uint32_t)p->v;
    pc[4] = p->base;
    pc[5] = (uint32_t)ramp;
    pc[6] = (uint32_t)(ramp >> 32);
    pc[7] = p->por & 8u;
    return run_commands(noise_record, pc);
}

/* the chip's bitplanes and the target (pack.comp), the RAM copied across */
typedef struct {
    uint32_t pc[7], cols, rows;
} PackJob;

static void pack_record(void *arg)
{
    PackJob *j = (PackJob *)arg;
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
    pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_pack);
    pvkCmdPushConstants(cb, pl, STAGES, 0, sizeof j->pc, j->pc);
    pvkCmdDispatch(cb, (j->cols + 7) / 8, (j->rows + 7) / 8, 1);
    barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
}

static void bits_move(uint8_t *tex, uint8_t *ram, const FwBits *b, int pack)
{
    size_t size = (size_t)b->mask + 1;
    PackJob j;
    if (tex != (uint8_t *)bufs[B_OUT].at || !buf_grow(B_BITS, size)) {
        fw_bits_cpu(tex, ram, b, pack);
        return;
    }
    memcpy(bufs[B_BITS].at, ram, size);
    if (dset_stale)
        dset_update();
    j.pc[0] = (uint32_t)b->w;
    j.pc[1] = (uint32_t)b->h;
    j.pc[2] = (uint32_t)b->bpp;
    j.pc[3] = (uint32_t)b->ht;
    j.pc[4] = b->base;
    j.pc[5] = b->mask;
    j.pc[6] = (uint32_t)pack;
    j.cols = (uint32_t)b->w / 8;
    j.rows = (uint32_t)b->h / 8;
    if (!run_commands(pack_record, &j)) {
        fw_bits_cpu(tex, ram, b, pack);
        return;
    }
    if (pack)
        memcpy(ram, bufs[B_BITS].at, size);
}

static void fill(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    static uint8_t *ref;
    static size_t ref_len;
    static FwSpan *run;
    static size_t run_cap;
    size_t i, j, len = (size_t)w * h;

    if (out != (uint8_t *)bufs[B_OUT].at || len > bufs[B_OUT].len || w % 4) {
        fw_span_fill_cpu(out, w, h, sp, n);     /* not the shared picture: the CPU's */
        return;
    }
    if (getenv("FW_GPUCHECK")) {
        if (ref_len < len) {
            free(ref);
            ref = malloc(len);
            ref_len = ref ? len : 0;
        }
        if (ref) {
            memcpy(ref, out, len);
            fw_span_fill_cpu(ref, w, h, sp, n);
        }
    }
    if (run_cap < n) {
        free(run);
        run = malloc(n * sizeof *run);
        run_cap = run ? n : 0;
        if (!run) {
            fw_span_fill_cpu(out, w, h, sp, n);
            return;
        }
    }
    id_clear = 1;
    /* in their order: runs of spans by the compute fill, runs of the
       display list's faces by the render pass (gpu_metal.m's) */
    for (i = 0; i < n;) {
        size_t m = 0;
        if (sp[i].textured == FW_HAZE) {    /* an effect's haze: what is drawn so far read (a marker: the runs broken here) */
            if (!haze_run(w, h, &sp[i]))
                fw_span_fill_cpu(out, w, h, &sp[i], 1);
            i++;
            continue;
        }
        if (sp[i].textured == FW_NOISE) {   /* a job's noise (a marker too) */
            if (!noise_run(w, h, &sp[i]))
                fw_span_fill_cpu(out, w, h, &sp[i], 1);
            i++;
            continue;
        }
        if (gpu_bin_face(&sp[i])) {
            for (j = i; j < n && gpu_bin_face(&sp[j]); j++)
                ;
            if (!draw_faces(w, h, sp + i, j - i))
                fprintf(stderr, "gpu_vulkan: faces not drawn\n");
            i = j;
            continue;
        }
        for (j = i; j < n && !gpu_bin_face(&sp[j]) && !fw_span_kernel(&sp[j]); j++)
            if (!gpu_bin_ref_only(&sp[j]))
                run[m++] = sp[j];
        if (!fill_spans(w, h, run, m))
            fw_span_fill_cpu(out, w, h, run, m);
        i = j;
    }
    if (getenv("FW_GPUCHECK") && ref) {
        if (!check.fills)
            atexit(check_report);
        gpu_bin_dump(check.fills, ref, out, getenv("FW_GPUDUMP") ? face_ids(w, h) : NULL, w, h);
        gpu_bin_check(&check, out, ref, w, h);
    }
}

/* ---- the water pass ---- */
static long water_fills, water_apart;

static void water_report(void)
{
    fprintf(stderr, "gpucheck: %ld water passes, %ld texels apart from the CPU's\n", water_fills, water_apart);
}

static uint32_t *texels32_alloc(size_t len)
{
    if (!buf_grow(B_OUT32, len))
        return NULL;
    return (uint32_t *)bufs[B_OUT32].at;
}

/* the water pass (fullwindow.h) on the GPU: the shared pictures only */
static void water_fill(const uint8_t *in, uint32_t *out, const FwWaterJob *j, const uint32_t *gap)
{
    size_t len = (size_t)j->w * (size_t)j->h;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};

    if (in != (const uint8_t *)bufs[B_OUT].at || out != (uint32_t *)bufs[B_OUT32].at || len > bufs[B_OUT].len ||
        len * 4 > bufs[B_OUT32].len || !buf_grow(B_JOB, sizeof *j) || !buf_grow(B_GAP, (size_t)j->h * 8)) {
        fw_water_fill_cpu(in, out, j, gap);
        return;
    }
    memcpy(bufs[B_JOB].at, j, sizeof *j);
    memcpy(bufs[B_GAP].at, gap, (size_t)j->h * 8);
    if (dset_stale)
        dset_update();
    pvkResetCommandBuffer(cb, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    pvkBeginCommandBuffer(cb, &bi);
    pvkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, NULL);
    pvkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_water);
    pvkCmdDispatch(cb, (uint32_t)((j->w + 7) / 8), (uint32_t)((j->h + 7) / 8), 1);
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
                          NULL);
    pvkEndCommandBuffer(cb);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    pvkResetFences(dev, 1, &fence);
    if (pvkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS ||
        pvkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull) != VK_SUCCESS) {
        fw_water_fill_cpu(in, out, j, gap);
        return;
    }
    if (getenv("FW_GPUCHECK")) {
        static uint32_t *ref;
        static size_t ref_len;
        size_t i;
        long apart = 0;
        if (ref_len < len) {
            free(ref);
            ref = malloc(len * 4);
            ref_len = ref ? len : 0;
        }
        if (ref) {
            fw_water_fill_cpu(in, ref, j, gap);
            for (i = 0; i < len; i++)
                apart += out[i] != ref[i];
            if (!water_fills++)
                atexit(water_report);
            water_apart += apart;
        }
    }
}

/* the GPU's fill in fw_span_filler's place, when there is a Vulkan GPU */
int gpu_vulkan_init(void)
{
    int i;
    if (!lib_load() || !device_open()) {
        fprintf(stderr, "stuntracefx: no Vulkan GPU with 64-bit integers: the CPU draws the full window\n");
        return 0;
    }
    for (i = 0; i < 256; i++)
        bank_slot[i] = -1;
    if (!buf_grow(B_BANKS, (size_t)SLOTS * 65536) || !buf_grow(B_SLOT, sizeof bank_slot))
        return 0;
    fw_texels_alloc = texels_alloc;
    fw_span_filler = fill;
    fw_texels32_alloc = texels32_alloc;
    fw_water_filler = water_fill;
    fw_bits_filler = bits_move;
    return 1;
}
