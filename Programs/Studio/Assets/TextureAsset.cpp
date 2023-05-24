/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file TextureAsset.cpp
* @author JXMaster
* @date 2020/5/9
*/
#pragma once
#include "Texture.hpp"
#include <RHI/RHI.hpp>
#include <Runtime/File.hpp>
#include <Image/Image.hpp>
#include <VFS/VFS.hpp>

namespace Luna
{
	struct TextureAssetUserdata
	{
		lustruct("TextureAssetUserdata", "{816CDA20-AB1C-4E24-A7CE-59E2EFE9BE1E}");

		Ref<RHI::IDescriptorSetLayout> m_mipmapping_dlayout;
		Ref<RHI::IShaderInputLayout> m_mipmapping_slayout;
		Ref<RHI::IPipelineState> m_mipmapping_pso;

		static constexpr u32 ENV_MAP_MIPS_COUNT = 5;

		RV init();

		RV generate_mipmaps(RHI::ITexture* resource_with_most_detailed_mip, RHI::ICommandBuffer* compute_cmdbuf);
	};
	RV TextureAssetUserdata::init()
	{
		using namespace RHI;
		lutry
		{
			{
				luset(m_mipmapping_dlayout, RHI::get_main_device()->new_descriptor_set_layout(DescriptorSetLayoutDesc({
					DescriptorSetLayoutBinding(DescriptorType::uniform_buffer_view, 0, 1, ShaderVisibilityFlag::all),
					DescriptorSetLayoutBinding(DescriptorType::read_texture_view, 1, 1, ShaderVisibilityFlag::all),
					DescriptorSetLayoutBinding(DescriptorType::read_write_texture_view, 2, 1, ShaderVisibilityFlag::all),
					DescriptorSetLayoutBinding(DescriptorType::sampler, 3, 1, ShaderVisibilityFlag::all)
					})));
				auto dlayout = m_mipmapping_dlayout.get();
				luset(m_mipmapping_slayout, RHI::get_main_device()->new_shader_input_layout(ShaderInputLayoutDesc(
					{ &dlayout, 1 },
					ShaderInputLayoutFlag::deny_vertex_shader_access |
					
					ShaderInputLayoutFlag::deny_pixel_shader_access)));

				lulet(psf, open_file("MipmapGenerationCS.cso", FileOpenFlag::read, FileCreationMode::open_existing));
				auto file_size = psf->get_size();
				auto cs_blob = Blob((usize)file_size);
				luexp(psf->read(cs_blob.span()));
				psf = nullptr;
				ComputePipelineStateDesc ps_desc;
				ps_desc.shader_input_layout = m_mipmapping_slayout;
				ps_desc.cs = cs_blob.cspan();
				luset(m_mipmapping_pso, RHI::get_main_device()->new_compute_pipeline_state(ps_desc));
			}
		}
		lucatchret;
		return ok;
	}
	RV TextureAssetUserdata::generate_mipmaps(RHI::ITexture* resource_with_most_detailed_mip, RHI::ICommandBuffer* compute_cmdbuf)
	{
		using namespace RHI;
		lutry
		{
			auto desc = resource_with_most_detailed_mip->get_desc();
			lucheck(desc.mip_levels);
			lucheck(desc.type == TextureType::tex2d);
			lucheck(desc.depth == 1);

			if (desc.mip_levels == 1)
			{
				return ok;
			}

			auto device = g_env->device;
			compute_cmdbuf->set_context(CommandBufferContextType::compute);
			compute_cmdbuf->set_compute_shader_input_layout(m_mipmapping_slayout);
			compute_cmdbuf->set_compute_pipeline_state(m_mipmapping_pso);
			u32 cb_align = device->get_uniform_buffer_data_alignment();
			u32 cb_size = (u32)align_upper(sizeof(Float2), cb_align);
			lulet(cb, device->new_buffer(MemoryType::upload,
				BufferDesc(BufferUsageFlag::uniform_buffer, cb_size * (desc.mip_levels - 1))));

			lulet(mapped, cb->map(0, 0));
			for (u32 j = 0; j < (u32)(desc.mip_levels - 1); ++j)
			{
				u32 width = max<u32>((u32)desc.width >> (j + 1), 1);
				u32 height = max<u32>(desc.height >> (j + 1), 1);
				Float2U* dest = (Float2U*)((usize)mapped + cb_size * j);
				dest->x = 1.0f / (f32)width;
				dest->y = 1.0f / (f32)height;
			}
			cb->unmap(0, USIZE_MAX);

			u32 width = desc.width / 2;
			u32 height = desc.height / 2;

			for (u32 j = 0; j < (u32)(desc.mip_levels - 1); ++j)
			{
				TextureBarrier barriers[] = {
					{resource_with_most_detailed_mip, SubresourceIndex(j, 0), TextureStateFlag::automatic, TextureStateFlag::shader_read_cs, ResourceBarrierFlag::none},
					{resource_with_most_detailed_mip, SubresourceIndex(j + 1, 0),TextureStateFlag::automatic, TextureStateFlag::shader_write_cs, ResourceBarrierFlag::none}
				};
				compute_cmdbuf->resource_barrier({}, { barriers, 2 });
				lulet(vs, device->new_descriptor_set(DescriptorSetDesc(m_mipmapping_dlayout)));
				vs->update_descriptors({
					WriteDescriptorSet::uniform_buffer_view(0, BufferViewDesc::uniform_buffer(cb, cb_size * j, cb_size)),
					WriteDescriptorSet::read_texture_view(1, TextureViewDesc::tex2d(resource_with_most_detailed_mip, Format::unknown, j, 1)),
					WriteDescriptorSet::read_write_texture_view(2, TextureViewDesc::tex2d(resource_with_most_detailed_mip, Format::unknown, j + 1, 1)),
					WriteDescriptorSet::sampler(3, SamplerDesc(Filter::min_mag_mip_linear, TextureAddressMode::clamp, TextureAddressMode::clamp, TextureAddressMode::clamp))
					});
				compute_cmdbuf->set_compute_descriptor_set(0, vs);
				compute_cmdbuf->attach_device_object(vs);
				compute_cmdbuf->dispatch(align_upper(width, 8) / 8, align_upper(height, 8) / 8, 1);
				width = max<u32>(width / 2, 1);
				height = max<u32>(height / 2, 1);
			}

			luexp(compute_cmdbuf->submit({}, {}, true));
			compute_cmdbuf->wait();
			luexp(compute_cmdbuf->reset());
		}
		lucatchret;
		return ok;
	}
	static R<ObjRef> load_texture_asset(object_t userdata, Asset::asset_t asset, const Path& path)
	{
		ObjRef ret;
		lutry
		{
			// Open image file.
			Path file_path = path;
			file_path.append_extension("tex");
			lulet(file, VFS::open_file(file_path, FileOpenFlag::read, FileCreationMode::open_existing));
			lulet(file_data, load_file_data(file));
			// Check whether the texture contains mips.
			if (file_data.size() >= 8 && !memcmp((const c8*)file_data.data(), "LUNAMIPS", 8))
			{
				u64* dp = (u64*)(file_data.data() + 8);
				u64 num_mips = *dp;
				++dp;
				Vector<Pair<u64, u64>> mip_descs;
				mip_descs.reserve(num_mips);
				for (u64 i = 0; i < num_mips; ++i)
				{
					Pair<u64, u64> p;
					p.first = dp[i * 2];
					p.second = dp[i * 2 + 1];
					mip_descs.push_back(p);
				}
				// Load texture from file.
				lulet(desc, Image::read_image_file_desc(file_data.data() + mip_descs[0].first, mip_descs[0].second));
				auto desired_format = get_desired_format(desc.format);
				// Create resource.
				lulet(tex, RHI::get_main_device()->new_texture(RHI::MemoryType::local, RHI::TextureDesc::tex2d(
					get_format_from_image_format(desc.format), 
					RHI::TextureUsageFlag::read_texture | RHI::TextureUsageFlag::read_write_texture | RHI::TextureUsageFlag::copy_source | RHI::TextureUsageFlag::copy_dest, 
					desc.width, desc.height)));
				// Upload data
				for (u64 i = 0; i < num_mips; ++i)
				{
					Image::ImageDesc desc;
					lulet(image_data, Image::read_image_file(file_data.data() + mip_descs[i].first, mip_descs[i].second, desired_format, desc));
					luexp(upload_texture_data(tex, RHI::SubresourceIndex(i, 0), 0, 0, 0, image_data.data(), pixel_size(desc.format) * desc.width, pixel_size(desc.format) * desc.width * desc.height,
						desc.width, desc.height, 1));
				}
				ret = tex;
			}
			else
			{
				// Load texture from file.
				lulet(desc, Image::read_image_file_desc(file_data.data(), file_data.size()));
				auto desired_format = get_desired_format(desc.format);
				lulet(image_data, Image::read_image_file(file_data.data(), file_data.size(), desired_format, desc));
				// Create resource.
				lulet(tex, RHI::get_main_device()->new_texture(RHI::MemoryType::local, RHI::TextureDesc::tex2d(
					get_format_from_image_format(desc.format),
					RHI::TextureUsageFlag::read_texture | RHI::TextureUsageFlag::read_write_texture | RHI::TextureUsageFlag::copy_source | RHI::TextureUsageFlag::copy_dest,
					desc.width, desc.height)));
				// Upload data.
				luexp(upload_texture_data(tex, RHI::SubresourceIndex(0, 0), 0, 0, 0, image_data.data(), pixel_size(desc.format) * desc.width, pixel_size(desc.format) * desc.width * desc.height,
					desc.width, desc.height, 1));
				// Generate mipmaps.
				Ref<TextureAssetUserdata> ctx = ObjRef(userdata);
				lulet(cmdbuf, g_env->device->new_command_buffer(g_env->async_compute_queue));
				luexp(ctx->generate_mipmaps(tex, cmdbuf));
				ret = tex;
			}
		}
		lucatchret;
		return ret;
	}
	RV register_static_texture_asset_type()
	{
		lutry
		{
			register_boxed_type<TextureAssetUserdata>();
			Ref<TextureAssetUserdata> userdata = new_object<TextureAssetUserdata>();
			luexp(userdata->init());
			Asset::AssetTypeDesc desc;
			desc.name = get_static_texture_asset_type();
			desc.on_load_asset = load_texture_asset;
			desc.on_save_asset = nullptr;
			desc.on_set_asset_data = nullptr;
			desc.userdata = userdata;
			Asset::register_asset_type(desc);
		}
		lucatchret;
		return ok;
	}
	Name get_static_texture_asset_type()
	{
		return "Static Texture";
	}
}