#include <unordered_set>

#include "USD/hgiVk/buffer.h"
#include "USD/hgiVk/commandBuffer.h"
#include "USD/hgiVk/conversions.h"
#include "USD/hgiVk/device.h"
#include "USD/hgiVk/diagnostic.h"
#include "USD/hgiVk/resourceBindings.h"
#include "USD/hgiVk/texture.h"
#include "USD/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkResourceBindings::HgiVkResourceBindings(
    HgiVkDevice* device,
    HgiResourceBindingsDesc const& desc)
    : HgiResourceBindings(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkDescriptorSetLayout(nullptr)
    , _vkDescriptorSet(nullptr)
    , _vkPipelineLayout(nullptr)
{
    // initialize the pool sizes for each descriptor type we support
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.resize(HgiBindResourceTypeCount);

    for (size_t i=0; i<HgiBindResourceTypeCount; i++) {
        HgiBindResourceType bt = HgiBindResourceType(i);
        VkDescriptorPoolSize p;
        p.descriptorCount = 0;
        p.type = HgiVkConversions::GetDescriptorType(bt);
        poolSizes[i] = p;
    }

    //
    // Create DescriptorSetLayout to describe resource bindings
    //
    // The descriptors are reference by shader code. E.g.
    //   layout (set=0, binding=0) uniform sampler2D...
    //   layout (std140, binding=1) uniform buffer{}
    //
    std::unordered_set<uint32_t> bindingsVisited;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (HgiTextureBindDesc const& t : desc.textures) {
        VkDescriptorSetLayoutBinding d = {};
        uint32_t bi = t.bindingIndex;
        if (bindingsVisited.find(bi)!=bindingsVisited.end()) {
            TF_WARN("Binding index must be unique in descriptor set.");
        }
        d.binding = bi; // binding number in shader stage
        bindingsVisited.insert(bi);
        d.descriptorType = HgiVkConversions::GetDescriptorType(t.resourceType);
        poolSizes[t.resourceType].descriptorCount++;
        d.descriptorCount = (uint32_t) t.textures.size();
        d.stageFlags = HgiVkConversions::GetShaderStages(t.stageUsage);
        d.pImmutableSamplers = nullptr;
        bindings.emplace_back(std::move(d));
    }

    for (HgiBufferBindDesc const& b : desc.buffers) {
        VkDescriptorSetLayoutBinding d = {};
        uint32_t bi = b.bindingIndex;
        if (bindingsVisited.find(bi)!=bindingsVisited.end()) {
            TF_WARN("Binding index must be unique in descriptor set.");
        }
        d.binding = bi; // binding number in shader stage
        bindingsVisited.insert(bi);
        d.descriptorType = HgiVkConversions::GetDescriptorType(b.resourceType);
        poolSizes[b.resourceType].descriptorCount++;
        d.descriptorCount = (uint32_t) b.buffers.size();
        d.stageFlags = HgiVkConversions::GetShaderStages(b.stageUsage);
        d.pImmutableSamplers = nullptr;
        bindings.emplace_back(std::move(d));
    }

    // Check descriptor indexing support.
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures =
        _device->GetVulkanPhysicalDeviceIndexingFeatures();
    VkDescriptorBindingFlagsEXT bindFlags = 0;

    // We use descriptor indexing, meaning certain arrays of textures are
    // dynamic in size. We have to provide the 'variable' flag for those arrays.
    if (indexingFeatures.descriptorBindingVariableDescriptorCount) {
        bindFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
    }

    // We also set the 'partially bound' flag so that the set may have invalid
    // descriptors in it, as long as a shader is not using them.
    if (indexingFeatures.descriptorBindingPartiallyBound) {
        bindFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
    }

    // XXX the following flags are also interesting as they give us more
    // flexiblity to update descriptor sets. We skip them for now.
    // VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT
    // VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    setLayoutBindingFlags.bindingCount = (uint32_t) bindings.size();
    std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags(
        bindings.size(), bindFlags);
    setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

    // Create descriptor
    VkDescriptorSetLayoutCreateInfo setCreateInfo =
        {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setCreateInfo.bindingCount = (uint32_t) bindings.size();
    setCreateInfo.pBindings = bindings.data();
    setCreateInfo.pNext = bindFlags ? &setLayoutBindingFlags : nullptr;

    TF_VERIFY(
        vkCreateDescriptorSetLayout(
            _device->GetVulkanDevice(),
            &setCreateInfo,
            HgiVkAllocator(),
            &_vkDescriptorSetLayout) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Descriptor Set Layout " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkDescriptorSetLayout,
            VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT,
            debugLabel.c_str());
    }

    //
    // Create the descriptor pool.
    //
    // XXX For now each resource bindings gets its own pool to allocate its
    // descriptor set from. We can't have a global descriptor pool since we have
    // multiple threads creating resourceBindings. On top of that, when the
    // resourceBindings gets destroyed it must de-allocate its descriptor set
    // in the correct descriptor pool (which is different than command buffers
    // where the entire pool is reset at the beginning of a new frame).
    //
    // If having a descriptor pool per resourceBindings turns out to be too much
    // overhead (e.g. if many resourceBindings are created/destroyed each frame)
    // then we can consider an approach similar to thread local command buffers.
    // We could allocate larger descriptor pools per frame and per thread.
    //

    for (size_t i=poolSizes.size(); i-- > 0;) {
        // Remove empty descriptorPoolSize or vulkan validation will complain
        if (poolSizes[i].descriptorCount == 0) {
            std::iter_swap(poolSizes.begin() + i, poolSizes.end() - 1);
            poolSizes.pop_back();
        }
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1; // Each resourceBinding has own pool -- read above
    pool_info.poolSizeCount = (uint32_t) poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    TF_VERIFY(
        vkCreateDescriptorPool(
            _device->GetVulkanDevice(),
            &pool_info,
            HgiVkAllocator(),
            &_vkDescriptorPool) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Descriptor Pool " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkDescriptorPool,
            VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT,
            debugLabel.c_str());
    }

    //
    // Create Descriptor Set
    //
    VkDescriptorSetAllocateInfo allocateInfo =
        {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};

    allocateInfo.descriptorPool = _vkDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &_vkDescriptorSetLayout;

    TF_VERIFY(
        vkAllocateDescriptorSets(
            _device->GetVulkanDevice(),
            &allocateInfo,
            &_vkDescriptorSet) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Descriptor Set " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkDescriptorSet,
            VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT,
            debugLabel.c_str());
    }

    //
    // Setup limits for each resource type
    //
    VkPhysicalDeviceProperties const& devProps =
        _device->GetVulkanPhysicalDeviceProperties();
    VkPhysicalDeviceLimits const& limits = devProps.limits;

    uint32_t bindLimits[HgiBindResourceTypeCount][2] = {
        {HgiBindResourceTypeSampler,
            limits.maxPerStageDescriptorSamplers},
        {HgiBindResourceTypeCombinedImageSampler,
            0 /*Should use SamplerImage limits below*/},
        {HgiBindResourceTypeSamplerImage,
            limits.maxPerStageDescriptorSampledImages},
        {HgiBindResourceTypeStorageImage,
            limits.maxPerStageDescriptorStorageImages},
        {HgiBindResourceTypeUniformBuffer,
            limits.maxPerStageDescriptorUniformBuffers},
        {HgiBindResourceTypeStorageBuffer,
            limits.maxPerStageDescriptorStorageBuffers}
    };


    //
    // Textures
    //

    std::vector<VkWriteDescriptorSet> writeSets;

    _imageInfos.clear();

    for (size_t i=0; i<desc.textures.size(); i++) {
        HgiTextureBindDesc const& texDesc = desc.textures[i];

        uint32_t & limit = bindLimits[HgiBindResourceTypeSamplerImage][1];
        if (!TF_VERIFY(limit>0, "Maximum array-of-texture/samplers exceeded")) {
            break;
        }
        limit -= 1;

        for (HgiTextureHandle const& texHandle : texDesc.textures) {
            HgiVkTexture* tex = static_cast<HgiVkTexture*>(texHandle);
            if (!TF_VERIFY(tex)) continue;
            VkDescriptorImageInfo imageInfo;
            imageInfo.sampler = tex->GetSampler();
            imageInfo.imageLayout = tex->GetImageLayout();
            imageInfo.imageView = tex->GetImageView();
            _imageInfos.emplace_back(std::move(imageInfo));
        }

        // For dstBinding we must provided an index in descriptor set.
        // Must be one of the bindings specified in VkDescriptorSetLayoutBinding
        VkWriteDescriptorSet writeSet= {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstBinding = texDesc.bindingIndex; // index in descriptor set
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = (uint32_t) texDesc.textures.size();
        writeSet.dstSet = _vkDescriptorSet;
        writeSet.pBufferInfo = nullptr;
        writeSet.pImageInfo = _imageInfos.data();
        writeSet.pTexelBufferView = nullptr;
        writeSet.descriptorType =
            HgiVkConversions::GetDescriptorType(texDesc.resourceType);
        writeSets.emplace_back(std::move(writeSet));
    }

    //
    // Buffers
    //

    _bufferInfos.clear();

    for (size_t i=0; i<desc.buffers.size(); i++) {
        HgiBufferBindDesc const& bufDesc = desc.buffers[i];

        uint32_t & limit = bindLimits[bufDesc.resourceType][1];
        if (!TF_VERIFY(limit>0, "Maximum size array-of-buffers exceeded")) {
            break;
        }
        limit -= 1;

        TF_VERIFY(bufDesc.buffers.size() == bufDesc.offsets.size());

        for (HgiBufferHandle const& bufHandle : bufDesc.buffers) {
            HgiVkBuffer* buf = static_cast<HgiVkBuffer*>(bufHandle);
            if (!TF_VERIFY(buf)) continue;
            VkDescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = buf->GetBuffer();
            bufferInfo.offset = bufDesc.offsets[i];
            bufferInfo.range = VK_WHOLE_SIZE;
            _bufferInfos.emplace_back(std::move(bufferInfo));
        }

        VkWriteDescriptorSet writeSet= {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstBinding = bufDesc.bindingIndex; // index in descriptor set
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = (uint32_t) bufDesc.buffers.size();
        writeSet.dstSet = _vkDescriptorSet;
        writeSet.pBufferInfo = _bufferInfos.data();
        writeSet.pImageInfo = nullptr;
        writeSet.pTexelBufferView = nullptr;
        writeSet.descriptorType =
            HgiVkConversions::GetDescriptorType(bufDesc.resourceType);
        writeSets.emplace_back(std::move(writeSet));
    }

    // Note: this update happens immediate. It is not recorded via a command.
    // This means we should only do this if the descriptorSet is not currently
    // in use on GPU. With 'descriptor indexing' extension this has relaxed a
    // little and we are allowed to use vkUpdateDescriptorSets before
    // vkBeginCommandBuffer and after vkEndCommandBuffer, just not during the
    // command buffer recording.
    vkUpdateDescriptorSets(
        _device->GetVulkanDevice(),
        (uint32_t) writeSets.size(),
        writeSets.data(),
        0,        // copy count
        nullptr); // copy_desc

    //
    // Pipeline layout contains descriptor set layouts and push constant ranges.
    //

    std::vector<VkPushConstantRange> pcRanges;
    for (HgiPushConstantDesc const& pcDesc : desc.pushConstants) {
        TF_VERIFY(pcDesc.byteSize % 4 == 0, "Push constants not multipes of 4");
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = pcDesc.offset;
        pushConstantRange.size = pcDesc.byteSize;
        pushConstantRange.stageFlags =
            HgiVkConversions::GetShaderStages(pcDesc.stageUsage);

        pcRanges.emplace_back(std::move(pushConstantRange));
    }

    VkPipelineLayoutCreateInfo pipeLayCreateInfo =
        {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeLayCreateInfo.pushConstantRangeCount = (uint32_t) pcRanges.size();
    pipeLayCreateInfo.pPushConstantRanges = pcRanges.data();
    pipeLayCreateInfo.setLayoutCount = 1;
    pipeLayCreateInfo.pSetLayouts = &_vkDescriptorSetLayout;

    TF_VERIFY(
        vkCreatePipelineLayout(
            _device->GetVulkanDevice(),
            &pipeLayCreateInfo,
            HgiVkAllocator(),
            &_vkPipelineLayout) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Pipeline Layout " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkPipelineLayout,
            VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT,
            debugLabel.c_str());
    }
}

HgiVkResourceBindings::~HgiVkResourceBindings()
{
    vkDestroyDescriptorSetLayout(
        _device->GetVulkanDevice(),
        _vkDescriptorSetLayout,
        HgiVkAllocator());

    vkDestroyPipelineLayout(
        _device->GetVulkanDevice(),
        _vkPipelineLayout,
        HgiVkAllocator());

    // Since we have one pool for this resourceBindings we can reset the pool
    // instead of freeing the descriptorSet.
    //
    // if (_vkDescriptorSet) {
    //     vkFreeDescriptorSets(
    //         _device->GetVulkanDevice(),
    //         _vkDescriptorPool,
    //         1,
    //         &_vkDescriptorSet);
    // }
    //
    vkDestroyDescriptorPool(
        _device->GetVulkanDevice(),
        _vkDescriptorPool,
        HgiVkAllocator());
}

HgiBufferBindDescVector const&
HgiVkResourceBindings::GetBufferBindings() const
{
    return _descriptor.buffers;
}

HgiTextureBindDescVector const&
HgiVkResourceBindings::GetTextureBindings() const
{
    return _descriptor.textures;
}

HgiVertexBufferDescVector const&
HgiVkResourceBindings::GetVertexBuffers() const
{
    return _descriptor.vertexBuffers;
}

void
HgiVkResourceBindings::BindResources(HgiVkCommandBuffer* cb)
{
    VkPipelineBindPoint bindPoint =
        _descriptor.pipelineType == HgiPipelineTypeCompute ?
        VK_PIPELINE_BIND_POINT_COMPUTE :
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    // When binding new resources for the currently bound pipeline it may
    // 'disturb' previously bound resources (for a previous pipeline) that
    // are no longer compatible with the layout for the new pipeline.
    // This essentially unbinds the old resources.

    vkCmdBindDescriptorSets(
        cb->GetCommandBufferForRecoding(),
        bindPoint,
        _vkPipelineLayout,
        0, // firstSet
        1, // descriptorSetCount -- strict limits, see maxBoundDescriptorSets
        &_vkDescriptorSet,
        0, // dynamicOffset
        nullptr);
}

VkPipelineLayout
HgiVkResourceBindings::GetPipelineLayout() const
{
    return _vkPipelineLayout;
}

VkDescriptorSet
HgiVkResourceBindings::GetDescriptorSet() const
{
    return _vkDescriptorSet;
}

VkDescriptorImageInfoVector const&
HgiVkResourceBindings::GetImageInfos() const
{
    return _imageInfos;
}

VkDescriptorBufferInfoVector const&
HgiVkResourceBindings::GetBufferInfos() const
{
    return _bufferInfos;
}

PXR_NAMESPACE_CLOSE_SCOPE
