/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file DescriptorSet.cpp
* @author JXMaster
* @date 2023/7/17
*/
#include "DescriptorSet.hpp"
#include "Resource.hpp"
namespace Luna
{
    namespace RHI
    {
        RV DescriptorSet::init(const DescriptorSetDesc& desc)
        {
            m_layout = cast_object<DescriptorSetLayout>(desc.layout);
            usize num_arguments = m_layout->m_num_arguments;
            if(test_flags(m_layout->m_flags, DescriptorSetLayoutFlag::variable_descriptors))
            {
                num_arguments += desc.num_variable_descriptors;
            }
            m_buffer = box(m_device->m_device->newBuffer(sizeof(u64) * num_arguments, encode_resource_options(MemoryType::upload)));
            if(!m_buffer)
            {
                return BasicError::bad_platform_call();
            }
            m_bindings.assign(m_layout->m_bindings.size());
            for(usize i = 0; i < m_bindings.size(); ++i)
            {
                auto& dst = m_bindings[i];
                auto& src = m_layout->m_bindings[i];
                if(src.type != DescriptorType::sampler)
                {
                    dst.m_resources.assign(src.num_descs, nullptr);
                    switch(src.type)
                    {
                        case DescriptorType::uniform_buffer_view:
                        case DescriptorType::read_buffer_view:
                        case DescriptorType::read_texture_view:
                            dst.m_usages = MTL::ResourceUsageRead;
                            break;
                        case DescriptorType::read_write_buffer_view:
                        case DescriptorType::read_write_texture_view:
                            dst.m_usages = MTL::ResourceUsageRead | MTL::ResourceUsageWrite;
                            break;
                        default:
                            break;
                    }
                    if(test_flags(src.shader_visibility_flags, ShaderVisibilityFlag::vertex))
                    {
                        dst.m_render_stages |= MTL::RenderStageVertex;
                    }
                    if(test_flags(src.shader_visibility_flags, ShaderVisibilityFlag::pixel))
                    {
                        dst.m_render_stages |= MTL::RenderStageFragment;
                    }
                }
            }
            return ok;
        }
        RV DescriptorSet::update_descriptors(Span<const WriteDescriptorSet> writes)
        {
            lutry
            {
                u64* data = (u64*)m_buffer->contents();
                for(auto& write : writes)
                {
                    // Find the binding record index for this write.
                    usize binding_index = 0;
                    for(; binding_index < m_layout->m_bindings.size(); ++binding_index)
                    {
                        if(write.binding_slot == m_layout->m_bindings[binding_index].binding_slot)
                        {
                            break;
                        }
                    }
                    if(binding_index == m_layout->m_bindings.size())
                    {
                        return set_error(BasicError::bad_arguments(), "The specified binding number %d is not specified in the descriptor set layout.", write.binding_slot);
                    }
                    u64 argument_offset = m_layout->m_argument_offsets[binding_index] + write.first_array_index;
                    switch(write.type)
                    {
                        case DescriptorType::uniform_buffer_view:
                        for(usize i = 0; i < write.buffer_views.size(); ++i)
                        {
                            auto& view = write.buffer_views[i];
                            Buffer* buffer = cast_object<Buffer>(view.buffer->get_object());
                            u64 data_offset = view.first_element;
                            data[argument_offset + i] = buffer->m_buffer->gpuAddress() + data_offset;
                            m_bindings[binding_index].m_resources[write.first_array_index + i] = buffer->m_buffer.get();
                        }
                        break;
                        case DescriptorType::read_buffer_view:
                        case DescriptorType::read_write_buffer_view:
                        for(usize i = 0; i < write.buffer_views.size(); ++i)
                        {
                            auto& view = write.buffer_views[i];
                            Buffer* buffer = cast_object<Buffer>(view.buffer->get_object());
                            u64 data_offset = view.format == Format::unknown ? view.element_size * view.first_element : bits_per_pixel(view.format) * view.first_element / 8;
                            data[argument_offset + i] = buffer->m_buffer->gpuAddress() + data_offset;
                            m_bindings[binding_index].m_resources[write.first_array_index + i] = buffer->m_buffer.get();
                        }
                        break;
                        case DescriptorType::read_texture_view:
                        case DescriptorType::read_write_texture_view:
                        for(usize i = 0; i < write.texture_views.size(); ++i)
                        {
                            auto view = write.texture_views[i];
                            Texture* tex = cast_object<Texture>(view.texture->get_object());
                            validate_texture_view_desc(tex->m_desc, view);
                            if(require_view_object(tex->m_desc, view))
                            {
                                lulet(tex_view, tex->get_texture_view(view));
                                MTL::ResourceID id = tex_view->m_texture->gpuResourceID();
                                ((MTL::ResourceID*)data)[argument_offset + i] = id;
                            }
                            else
                            {
                                MTL::ResourceID id = tex->m_texture->gpuResourceID();
                                ((MTL::ResourceID*)data)[argument_offset + i] = id;
                            }
                            m_bindings[binding_index].m_resources[write.first_array_index + i] = tex->m_texture.get();
                        }
                        break;
                        case DescriptorType::sampler:
                        for(usize i = 0; i < write.samplers.size(); ++i)
                        {
                            const SamplerDesc& view = write.samplers[i];
                            NSPtr<MTL::SamplerDescriptor> sampler_desc = box(MTL::SamplerDescriptor::alloc()->init());
                            sampler_desc->setMinFilter(encode_min_mag_filter(view.min_filter));
                            sampler_desc->setMagFilter(encode_min_mag_filter(view.mag_filter));
                            sampler_desc->setMipFilter(encode_mip_filter(view.mip_filter));
                            if(view.anisotropy_enable)
                            {
                                sampler_desc->setMaxAnisotropy(view.max_anisotropy);
                            }
                            else
                            {
                                sampler_desc->setMaxAnisotropy(1);
                            }
                            if(view.compare_enable)
                            {
                                sampler_desc->setCompareFunction(encode_compare_function(view.compare_function));
                            }
                            else
                            {
                                sampler_desc->setCompareFunction(MTL::CompareFunctionNever);
                            }
                            sampler_desc->setLodMinClamp(view.min_lod);
                            sampler_desc->setLodMaxClamp(view.max_lod);
                            sampler_desc->setLodAverage(false);
                            switch(view.border_color)
                            {
                                case BorderColor::float_0000:
                                case BorderColor::int_0000:
                                sampler_desc->setBorderColor(MTL::SamplerBorderColorTransparentBlack);
                                break;
                                case BorderColor::float_0001:
                                case BorderColor::int_0001:
                                sampler_desc->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
                                break;
                                case BorderColor::float_1111:
                                case BorderColor::int_1111:
                                sampler_desc->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
                                break;
                            }
                            sampler_desc->setNormalizedCoordinates(true);
                            sampler_desc->setSAddressMode(encode_address_mode(view.address_u));
                            sampler_desc->setTAddressMode(encode_address_mode(view.address_v));
                            sampler_desc->setRAddressMode(encode_address_mode(view.address_w));
                            sampler_desc->setSupportArgumentBuffers(true);
                            NSPtr<MTL::SamplerState> sampler = box(m_device->m_device->newSamplerState(sampler_desc.get()));
                            if(!sampler)
                            {
                                return BasicError::bad_platform_call();
                            }
                            m_samplers.insert_or_assign(write.binding_slot + write.first_array_index + (u32)i, sampler);
                            ((MTL::ResourceID*)data)[argument_offset + i] = sampler->gpuResourceID();
                        }
                        break;
                    }
                }
            }
            lucatchret;
            return ok;
        }
    }
}