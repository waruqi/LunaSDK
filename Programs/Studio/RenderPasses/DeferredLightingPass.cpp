/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file DeferredLightingPass.cpp
* @author JXMaster
* @date 2023/3/18
*/
#include "DeferredLightingPass.hpp"
#include <Runtime/File.hpp>
#include "../Assets/SceneEditor.hpp"

namespace Luna
{
    RV DeferredLightingPassGlobalData::init(RHI::IDevice* device)
    {
        using namespace RHI;
        lutry
        {
            luset(m_deferred_lighting_pass_dlayout, device->new_descriptor_set_layout(DescriptorSetLayoutDesc({
						DescriptorSetLayoutBinding(DescriptorType::cbv, 0, 1, ShaderVisibility::all),
						DescriptorSetLayoutBinding(DescriptorType::srv, 1, 1, ShaderVisibility::all),
                        DescriptorSetLayoutBinding(DescriptorType::srv, 2, 1, ShaderVisibility::all),
                        DescriptorSetLayoutBinding(DescriptorType::srv, 3, 1, ShaderVisibility::all),
                        DescriptorSetLayoutBinding(DescriptorType::srv, 4, 1, ShaderVisibility::all),
                        DescriptorSetLayoutBinding(DescriptorType::srv, 5, 1, ShaderVisibility::all),
                        DescriptorSetLayoutBinding(DescriptorType::srv, 6, 1, ShaderVisibility::all),
						DescriptorSetLayoutBinding(DescriptorType::uav, 7, 1, ShaderVisibility::all),
						DescriptorSetLayoutBinding(DescriptorType::sampler, 8, 1, ShaderVisibility::all)
						})));

			luset(m_deferred_lighting_pass_slayout, device->new_shader_input_layout(ShaderInputLayoutDesc({ m_deferred_lighting_pass_dlayout },
				ShaderInputLayoutFlag::deny_vertex_shader_access |
				ShaderInputLayoutFlag::deny_domain_shader_access |
				ShaderInputLayoutFlag::deny_geometry_shader_access |
				ShaderInputLayoutFlag::deny_hull_shader_access |
				ShaderInputLayoutFlag::deny_pixel_shader_access)));

			lulet(psf, open_file("DeferredLighting.cso", FileOpenFlag::read, FileCreationMode::open_existing));
			auto file_size = psf->get_size();
			auto cs_blob = Blob((usize)file_size);
			luexp(psf->read(cs_blob.span()));
			psf = nullptr;
			ComputePipelineStateDesc ps_desc;
			ps_desc.cs = cs_blob.cspan();
			ps_desc.shader_input_layout = m_deferred_lighting_pass_slayout;
			luset(m_deferred_lighting_pass_pso, device->new_compute_pipeline_state(ps_desc));
        }
        lucatchret;
        return ok;
    }
    RV DeferredLightingPass::init(DeferredLightingPassGlobalData* global_data)
    {
        using namespace RHI;
        lutry
        {
            m_global_data = global_data;
            luset(m_ds, m_global_data->m_deferred_lighting_pass_dlayout->get_device()->new_descriptor_set(
                DescriptorSetDesc(global_data->m_deferred_lighting_pass_dlayout)));
        }
        lucatchret;
        return ok;
    }
    RV DeferredLightingPass::execute(RG::IRenderPassContext* ctx)
    {
        using namespace RHI;
        lutry
        {
            auto scene_tex = ctx->get_output("scene_texture");
            auto depth_tex = ctx->get_input("depth_texture");
            auto base_color_roughness_tex = ctx->get_input("base_color_roughness_texture");
            auto normal_metallic_tex = ctx->get_input("normal_metallic_texture");
			auto emissive_tex = ctx->get_input("emissive_texture");
            auto cmdbuf = ctx->get_command_buffer();
            auto device = cmdbuf->get_device();
            auto cb_align = device->get_constant_buffer_data_alignment();
            cmdbuf->resource_barriers({
                ResourceBarrierDesc::as_transition(camera_cb, ResourceState::vertex_and_constant_buffer),
                ResourceBarrierDesc::as_transition(light_params, ResourceState::shader_resource_non_pixel),
				ResourceBarrierDesc::as_transition(scene_tex, ResourceState::unordered_access),
				ResourceBarrierDesc::as_transition(depth_tex, ResourceState::shader_resource_non_pixel),
				ResourceBarrierDesc::as_transition(base_color_roughness_tex, ResourceState::shader_resource_non_pixel),
                ResourceBarrierDesc::as_transition(normal_metallic_tex, ResourceState::shader_resource_non_pixel),
                ResourceBarrierDesc::as_transition(emissive_tex, ResourceState::shader_resource_non_pixel)});
            
            m_ds->set_cbv(0, camera_cb, ConstantBufferViewDesc(0, (u32)align_upper(sizeof(CameraCB), cb_align)));
            if (light_ts.empty())
			{
				// Adds one fake light.
				m_ds->set_srv(1, light_params, &ShaderResourceViewDesc::as_buffer(Format::unknown, 0, 1, sizeof(LightingParams)));
			}
			else
			{
				m_ds->set_srv(1, light_params, &ShaderResourceViewDesc::as_buffer(Format::unknown, 0, (u32)light_ts.size(), sizeof(LightingParams)));
			}
            m_ds->set_srv(2, base_color_roughness_tex);
            m_ds->set_srv(3, normal_metallic_tex);
            m_ds->set_srv(4, emissive_tex);
            m_ds->set_srv(5, depth_tex, &ShaderResourceViewDesc::as_tex2d(Format::r32_float, 0, 1, 0.0f));
            m_ds->set_srv(6, skybox);
            m_ds->set_uav(7, scene_tex);
            m_ds->set_sampler(8, SamplerDesc(FilterMode::min_mag_mip_linear, TextureAddressMode::clamp, TextureAddressMode::clamp, TextureAddressMode::clamp));
            auto scene_desc = scene_tex->get_desc();
            cmdbuf->set_compute_shader_input_layout(m_global_data->m_deferred_lighting_pass_slayout);
            cmdbuf->set_pipeline_state(m_global_data->m_deferred_lighting_pass_pso);
            cmdbuf->set_compute_descriptor_set(0, m_ds);
            cmdbuf->dispatch((u32)align_upper(scene_desc.width_or_buffer_size, 8) / 8,
                align_upper(scene_desc.height, 8) / 8, 1);
        }
        lucatchret;
        return ok;
    }

    RV compile_deferred_lighting_pass(object_t userdata, RG::IRenderGraphCompiler* compiler)
    {
        lutry
        {
            DeferredLightingPassGlobalData* data = (DeferredLightingPassGlobalData*)userdata;
			auto scene_texture = compiler->get_output_resource("scene_texture");
			auto depth_texture = compiler->get_input_resource("depth_texture");
            auto base_color_roughness_texture = compiler->get_input_resource("base_color_roughness_texture");
            auto normal_metallic_texture = compiler->get_input_resource("normal_metallic_texture");
            auto emissive_texture = compiler->get_input_resource("emissive_texture");
			if(scene_texture == RG::INVALID_RESOURCE) return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Output \"scene_texture\" is not specified.");
			if(depth_texture == RG::INVALID_RESOURCE) return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Input \"depth_texture\" is not specified.");
            if(base_color_roughness_texture == RG::INVALID_RESOURCE) return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Input \"base_color_roughness_texture\" is not specified.");
            if(normal_metallic_texture == RG::INVALID_RESOURCE) return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Input \"normal_metallic_texture\" is not specified.");
            if(emissive_texture == RG::INVALID_RESOURCE) return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Input \"emissive_texture\" is not specified.");
			RHI::ResourceDesc desc = compiler->get_resource_desc(scene_texture);
			if (desc.pixel_format != RHI::Format::rgba32_float)
			{
				return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Invalid format for \"scene_texture\" is specified. \"scene_texture\" must be Format::rgba32_float.");
			}
			desc.usages |= RHI::ResourceUsageFlag::unordered_access;
			compiler->set_resource_desc(scene_texture, desc);

			desc = compiler->get_resource_desc(depth_texture);
			if (desc.pixel_format != RHI::Format::d32_float)
			{
				return set_error(BasicError::bad_arguments(), "DeferredLightingPass: Invalid format for \"depth_texture\" is specified. \"depth_texture\" must be Format::d32_float.");
			}
			desc.usages |= RHI::ResourceUsageFlag::depth_stencil;
			compiler->set_resource_desc(depth_texture, desc);

            desc = compiler->get_resource_desc(base_color_roughness_texture);
			desc.usages |= RHI::ResourceUsageFlag::shader_resource;
			compiler->set_resource_desc(base_color_roughness_texture, desc);

            desc = compiler->get_resource_desc(normal_metallic_texture);
			desc.usages |= RHI::ResourceUsageFlag::shader_resource;
			compiler->set_resource_desc(normal_metallic_texture, desc);

            desc = compiler->get_resource_desc(emissive_texture);
			desc.usages |= RHI::ResourceUsageFlag::shader_resource;
			compiler->set_resource_desc(emissive_texture, desc);

			Ref<DeferredLightingPass> pass = new_object<DeferredLightingPass>();
            luexp(pass->init(data));
			compiler->set_render_pass_object(pass);
        }
        lucatchret;
        return ok;
    }

    RV register_deferred_lighting_pass()
    {
        lutry
        {
            register_boxed_type<DeferredLightingPassGlobalData>();
            register_boxed_type<DeferredLightingPass>();
            impl_interface_for_type<DeferredLightingPass, RG::IRenderPass>();
            RG::RenderPassTypeDesc desc;
            desc.name = "DeferredLighting";
            desc.desc = "Illuminate the scene.";
            desc.output_parameters.push_back({"scene_texture", "The scene texture."});
            desc.input_parameters.push_back({"depth_texture", "The scene depth texture"});
            desc.input_parameters.push_back({"base_color_roughness_texture", "The base color and roughness texture from geometry pass."});
            desc.input_parameters.push_back({"normal_metallic_texture", "The normal and metallic texture from geometry pass."});
            desc.input_parameters.push_back({"emissive_texture", "The emissive texture from geometry pass."});
            desc.compile = compile_deferred_lighting_pass;
            auto data = new_object<DeferredLightingPassGlobalData>();
            luexp(data->init(RHI::get_main_device()));
            desc.userdata = data.object();
            RG::register_render_pass_type(desc);
        }
        lucatchret;
        return ok;
    }
}