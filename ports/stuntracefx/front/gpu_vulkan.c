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
    X(vkDeviceWaitIdle)
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
static VkPipeline pipe_rows, pipe_fill, pipe_water;
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
enum { B_OUT, B_SPANS, B_ROW_AT, B_ROW_SPANS, B_BANKS, B_SLOT, B_POLYS, B_EDGES, B_ROW_POLY, B_OUT32, B_JOB, B_GAP, B_N };
static const uint32_t binding_of[B_N] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13};
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
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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
        VkPushConstantRange pr = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 32};   /* w, h, 0, 0, the water's (fill.comp) */
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
            b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
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
    if (!pipe_rows || !pipe_fill || !pipe_water)
        return 0;
    for (i = 0; i < B_N; i++)
        if (!buf_grow((int)i, 16))
            return 0;
    return 1;
}

/* ---- the banks the texels come from ---- */
#define SLOTS 128
static uint8_t bank_ok[256];
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
    return 1;
}

/* ---- the fill ---- */
static long check_fills, check_apart;

static void check_report(void)
{
    fprintf(stderr, "gpucheck: %ld fills, %ld pixels apart from the CPU's\n", check_fills, check_apart);
}

static uint8_t *texels_alloc(size_t len)
{
    if (!buf_grow(B_OUT, len))
        return NULL;
    return (uint8_t *)bufs[B_OUT].at;
}

static void fill(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    static uint8_t *ref;
    static size_t ref_len;
    size_t i, len = (size_t)w * h;
    GpuBinSizes z;
    uint32_t pc[8] = {0};
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    int b;

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
    gpu_bin_sizes(sp, n, &z);
    if (!buf_grow(B_ROW_AT, ((size_t)h + 1) * 4) || !buf_grow(B_ROW_SPANS, z.total * 4) ||
        !buf_grow(B_SPANS, z.total * sizeof *sp) || !buf_grow(B_ROW_POLY, z.rows * 8) ||
        !buf_grow(B_POLYS, z.polys * sizeof(FwPoly)) || !buf_grow(B_EDGES, z.edges * sizeof(FwEdge)) ||
        !gpu_bin_fill(sp, n, h, (FwSpan *)bufs[B_SPANS].at, (uint32_t *)bufs[B_ROW_AT].at,
                      (uint32_t *)bufs[B_ROW_SPANS].at, (uint32_t *)bufs[B_ROW_POLY].at)) {
        fw_span_fill_cpu(out, w, h, sp, n);
        return;
    }
    if (z.polys)
        memcpy(bufs[B_POLYS].at, fw_polys, z.polys * sizeof(FwPoly));
    if (z.edges)
        memcpy(bufs[B_EDGES].at, fw_edges, z.edges * sizeof(FwEdge));
    {                                   /* ROM's banks once, RAM's ($70, $71) as they are now */
        uint8_t fresh[256] = {0};
        for (i = 0; i < n; i++) {
            if ((b = gpu_bin_bank(&sp[i])) < 0 || fresh[b] || (bank_ok[b] && b != 0x70 && b != 0x71))
                continue;
            if (!bank_put((uint8_t)b)) {
                fw_span_fill_cpu(out, w, h, sp, n);
                return;
            }
            fresh[b] = 1;
        }
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
        pvkCmdPushConstants(cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, pc);
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
    pvkCmdPushConstants(cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 32, pc);
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
        pvkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull) != VK_SUCCESS) {
        fw_span_fill_cpu(out, w, h, sp, n);
        return;
    }
    if (getenv("FW_GPUCHECK") && ref) {
        long apart = 0;
        for (i = 0; i < len; i++)
            apart += out[i] != ref[i];
        if (!check_fills++)
            atexit(check_report);
        check_apart += apart;
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
    return 1;
}
