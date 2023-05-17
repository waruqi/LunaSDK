/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file ShapeRenderer.cpp
* @author JXMaster
* @date 2022/4/25
*/
#include <Runtime/PlatformDefines.hpp>
#define LUNA_VG_API LUNA_EXPORT
#include "ShapeRenderer.hpp"
#include <Runtime/Math/Transform.hpp>
#include <RHI/ShaderCompileHelper.hpp>

namespace Luna
{
	namespace VG
	{
		Blob g_fill_shader_vs;
		Blob g_fill_shader_ps;
		Ref<RHI::IDescriptorSetLayout> g_fill_desc_layout;
		Ref<RHI::IShaderInputLayout> g_fill_slayout;
		Ref<RHI::ITexture> g_white_tex;

		RV init_render_resources()
		{
			using namespace RHI;
			auto dev = get_main_device();
			lutry
			{
				{
					auto compiler = ShaderCompiler::new_compiler();
					compiler->set_source({ FILL_SHADER_SOURCE_VS, FILL_SHADER_SOURCE_VS_SIZE });
					compiler->set_source_name("FillVS");
					compiler->set_entry_point("main");
					compiler->set_target_format(RHI::get_current_platform_shader_target_format());
					compiler->set_shader_type(ShaderCompiler::ShaderType::vertex);
					compiler->set_shader_model(6, 0);
					compiler->set_optimization_level(ShaderCompiler::OptimizationLevel::full);
					luexp(compiler->compile());
					auto data = compiler->get_output();
					g_fill_shader_vs = Blob(data.data(), data.size());

					compiler->reset();
					compiler->set_source({ FILL_SHADER_SOURCE_PS, FILL_SHADER_SOURCE_PS_SIZE });
					compiler->set_source_name("FillPS");
					compiler->set_entry_point("main");
					compiler->set_target_format(RHI::get_current_platform_shader_target_format());
					compiler->set_shader_type(ShaderCompiler::ShaderType::pixel);
					compiler->set_shader_model(6, 0);
					compiler->set_optimization_level(ShaderCompiler::OptimizationLevel::full);
					luexp(compiler->compile());
					data = compiler->get_output();
					g_fill_shader_ps = Blob(data.data(), data.size());
				}
				{
					DescriptorSetLayoutDesc desc({
						DescriptorSetLayoutBinding(DescriptorType::uniform_buffer_view, 0, 1, ShaderVisibilityFlag::vertex),
						DescriptorSetLayoutBinding(DescriptorType::read_buffer_view, 1, 1, ShaderVisibilityFlag::all),
						DescriptorSetLayoutBinding(DescriptorType::sampled_texture_view, 2, 1, ShaderVisibilityFlag::pixel),
						DescriptorSetLayoutBinding(DescriptorType::sampler, 3, 1, ShaderVisibilityFlag::pixel),
					});
					luset(g_fill_desc_layout, dev->new_descriptor_set_layout(desc));
				}
				{
					IDescriptorSetLayout* dl = g_fill_desc_layout;
					ShaderInputLayoutDesc desc ({&dl, 1},
						ShaderInputLayoutFlag::allow_input_assembler_input_layout
					);
					luset(g_fill_slayout, dev->new_shader_input_layout(desc));
				}
				{
					TextureDesc desc = TextureDesc::tex2d(ResourceHeapType::local, Format::rgba8_unorm, TextureUsageFlag::sampled_texture | TextureUsageFlag::copy_dest, 1, 1);
					luset(g_white_tex, dev->new_texture(desc));
					u32 data = 0xFFFFFFFF;

					{
						u64 size, row_pitch, slice_pitch;
						dev->get_texture_data_placement_info(1, 1, 1, Format::rgba8_unorm, &size, nullptr, &row_pitch, &slice_pitch);
						lulet(tex_staging, dev->new_buffer(BufferDesc(ResourceHeapType::upload, BufferUsageFlag::copy_source, size)));
						lulet(tex_staging_data, tex_staging->map(0, 0));
						memcpy(tex_staging_data, &data, sizeof(data));
						tex_staging->unmap(0, sizeof(data));
						u32 copy_queue_index = U32_MAX;
						{
							// Prefer a dedicated copy queue if present.
							u32 num_queues = dev->get_num_command_queues();
							for (u32 i = 0; i < num_queues; ++i)
							{
								auto desc = dev->get_command_queue_desc(i);
								if (desc.type == CommandQueueType::graphics && copy_queue_index == U32_MAX)
								{
									copy_queue_index = i;
								}
								else if (desc.type == CommandQueueType::copy)
								{
									copy_queue_index = i;
									break;
								}
							}
						}
						lulet(upload_cmdbuf, dev->new_command_buffer(copy_queue_index));
						upload_cmdbuf->resource_barrier({
							{ tex_staging, BufferStateFlag::automatic, BufferStateFlag::copy_source, ResourceBarrierFlag::none} },
							{ { g_white_tex, TEXTURE_BARRIER_ALL_SUBRESOURCES, TextureStateFlag::automatic, TextureStateFlag::copy_dest, ResourceBarrierFlag::discard_content } });
						upload_cmdbuf->copy_buffer_to_texture(g_white_tex, SubresourceIndex(0, 0), 0, 0, 0, tex_staging, 0,
							row_pitch, slice_pitch, 1, 1, 1);
						luexp(upload_cmdbuf->submit({}, {}, true));
						upload_cmdbuf->wait();
					}
				}
			}
			lucatchret;
			return ok;
		}
		void deinit_render_resources()
		{
			g_fill_shader_vs.clear();
			g_fill_shader_ps.clear();
			g_fill_desc_layout = nullptr;
			g_fill_slayout = nullptr;
			g_white_tex = nullptr;
		}
		RV FillShapeRenderer::create_pso(RHI::Format rt_format)
		{
			using namespace RHI;
			lutry
			{
				GraphicsPipelineStateDesc desc;
				desc.input_layout = InputLayoutDesc(
					{
						InputBindingDesc(0, sizeof(Vertex), InputRate::per_vertex)
					},
						{
							InputAttributeDesc("POSITION", 0, 0, 0, offsetof(Vertex, position), Format::rg32_float),
							InputAttributeDesc("SHAPECOORD", 0, 1, 0, offsetof(Vertex, shapecoord), Format::rg32_float),
							InputAttributeDesc("TEXCOORD", 0, 2, 0, offsetof(Vertex, texcoord), Format::rg32_float),
							InputAttributeDesc("COLOR", 0, 3, 0, offsetof(Vertex, color), Format::rgba8_unorm),
							InputAttributeDesc("COMMAND_OFFSET", 0, 4, 0, offsetof(Vertex, begin_command), Format::r32_uint),
							InputAttributeDesc("NUM_COMMANDS", 0, 5, 0, offsetof(Vertex, num_commands), Format::r32_uint),
						});
				desc.shader_input_layout = g_fill_slayout;
				desc.vs = { g_fill_shader_vs.data(), g_fill_shader_vs.size() };
				desc.ps = { g_fill_shader_ps.data(), g_fill_shader_ps.size() };
				desc.blend_state = BlendDesc({ AttachmentBlendDesc(true, BlendFactor::src_alpha, BlendFactor::inv_src_alpha, BlendOp::add, BlendFactor::zero,
						BlendFactor::one, BlendOp::add, ColorWriteMask::all) });
				desc.rasterizer_state = RasterizerDesc(FillMode::solid, CullMode::back, 0, 0.0f, 0.0f, 0, false, false, false, false, false);
				desc.depth_stencil_state = DepthStencilDesc(false, false);
				desc.num_render_targets = 1;
				desc.rtv_formats[0] = rt_format;
				luset(m_fill_pso, get_main_device()->new_graphics_pipeline_state(desc));
			}
			lucatchret;
			return ok;
		}
		RV FillShapeRenderer::init(RHI::ITexture* render_target)
		{
			return set_render_target(render_target);
		}
		void FillShapeRenderer::reset()
		{
			lutsassert();
		}
		RV FillShapeRenderer::set_render_target(RHI::ITexture* render_target)
		{
			lutsassert();
			auto desc = render_target->get_desc();
			lutry
			{
				if (m_rt_format != desc.pixel_format)
				{
					luexp(create_pso(desc.pixel_format));
					m_rt_format = desc.pixel_format;
				}
				luset(m_rtv, render_target->get_device()->new_render_target_view(render_target));
				m_render_target = render_target;
				m_screen_width = desc.width;
				m_screen_height = desc.height;
			}
			lucatchret;
			return ok;
		}
		RV FillShapeRenderer::render(
			RHI::ICommandBuffer* cmdbuf,
			RHI::IBuffer* shape_buffer,
			u32 num_points,
			RHI::IBuffer* vertex_buffer,
			u32 num_vertices,
			RHI::IBuffer* index_buffer,
			u32 num_indices,
			const ShapeDrawCall* draw_calls,
			u32 num_draw_calls
		)
		{
			using namespace RHI;
			lutsassert();
			auto dev = get_main_device();
			lutry
			{
				u32 cb_element_size = max<u32>((u32)dev->get_uniform_buffer_data_alignment(), (u32)sizeof(Float4x4U));
				u64 cb_size = cb_element_size * num_draw_calls;
				// Build constant buffer.
				if (num_draw_calls > m_cbs_capacity)
				{
					luset(m_cbs_resource, dev->new_buffer(BufferDesc(ResourceHeapType::upload, BufferUsageFlag::uniform_buffer,
						cb_size)));
					m_cbs_capacity = num_draw_calls;
				}
				lulet(cb_data, m_cbs_resource->map(0, 0));
				for (usize i = 0; i < num_draw_calls; ++i)
				{
					Float4x4U* dest = (Float4x4U*)(((usize)cb_data) + i * cb_element_size);
					Float4x4 transform = AffineMatrix::make_rotation_z(draw_calls[i].rotation / 180.0f * PI);
					transform = mul(transform, AffineMatrix::make_translation(Float3(draw_calls[i].origin_point.x, draw_calls[i].origin_point.y, 0.0f)));
					Float4x4 mat = ProjectionMatrix::make_orthographic_off_center(0.0f, (f32)m_screen_width, 0.0f, (f32)m_screen_height, 0.0f, 1.0f);
					mat = mul(transform, mat);
					*dest = mat;
				}
				m_cbs_resource->unmap(0, cb_size);
				// Build view sets.
				for (usize i = 0; i < num_draw_calls; ++i)
				{
					while (m_desc_sets.size() <= i)
					{
						lulet(desc_set, dev->new_descriptor_set(DescriptorSetDesc(g_fill_desc_layout)));
						m_desc_sets.push_back(desc_set);
					}
					auto& ds = m_desc_sets[i];

					luexp(ds->update_descriptors({
						DescriptorSetWrite::uniform_buffer_view(0, BufferViewDesc::uniform_buffer(m_cbs_resource)),
						DescriptorSetWrite::read_buffer_view(1, BufferViewDesc::typed_buffer(shape_buffer, 0, num_points, Format::r32_float)),
						DescriptorSetWrite::sampled_texture_view(2, TextureViewDesc::tex2d(draw_calls[i].texture ? draw_calls[i].texture : g_white_tex)),
						DescriptorSetWrite::sampler(3, SamplerDesc(Filter::min_mag_mip_linear, TextureAddressMode::clamp, TextureAddressMode::clamp, TextureAddressMode::clamp))
						}));
				}
				// Build command buffer.
				Vector<TextureBarrier> barriers;
				barriers.push_back({ m_render_target, SubresourceIndex(0, 0), TextureStateFlag::automatic, TextureStateFlag::color_attachment_write, ResourceBarrierFlag::discard_content });
				barriers.push_back({ g_white_tex, TEXTURE_BARRIER_ALL_SUBRESOURCES, TextureStateFlag::automatic, TextureStateFlag::shader_read_ps, ResourceBarrierFlag::none });
				for (usize i = 0; i < num_draw_calls; ++i)
				{
					if (draw_calls[i].texture)
					{
						barriers.push_back({ draw_calls[i].texture, TEXTURE_BARRIER_ALL_SUBRESOURCES, TextureStateFlag::automatic, TextureStateFlag::shader_read_ps, ResourceBarrierFlag::none });
					}
				}
				cmdbuf->resource_barrier({}, { barriers.data(), (u32)barriers.size()});
				RenderPassDesc desc;
				desc.color_attachments[0] = m_rtv;
				desc.color_load_ops[0] = LoadOp::clear;
				desc.color_store_ops[0] = StoreOp::store;
				desc.color_clear_values[0] = Float4U{ 0.0f };
				cmdbuf->begin_render_pass(desc);
				cmdbuf->set_pipeline_state(m_fill_pso);
				cmdbuf->set_graphics_shader_input_layout(g_fill_slayout);
				cmdbuf->set_vertex_buffers(0, { &VertexBufferView(vertex_buffer, 0, sizeof(Vertex) * num_vertices, sizeof(Vertex)), 1 });
				cmdbuf->set_index_buffer({index_buffer, 0, num_indices * sizeof(u32), Format::r32_uint});
				cmdbuf->set_viewport(Viewport(0.0f, 0.0f, (f32)m_screen_width, (f32)m_screen_height, 0.0f, 1.0f));
				for (usize i = 0; i < num_draw_calls; ++i)
				{
					IDescriptorSet* ds = m_desc_sets[i];
					cmdbuf->set_graphics_descriptor_sets(0, { &ds, 1 });
					if (draw_calls[i].clip_rect != RectI(0, 0, 0, 0))
					{
						cmdbuf->set_scissor_rect(draw_calls[i].clip_rect);
					}
					else
					{
						cmdbuf->set_scissor_rect(RectI(0, 0, m_screen_width, m_screen_height));
					}
					cmdbuf->draw_indexed(draw_calls[i].num_indices, draw_calls[i].base_index, 0);
				}
				cmdbuf->end_render_pass();
			}
			lucatchret;
			return ok;
		}
		LUNA_VG_API R<Ref<IShapeRenderer>> new_fill_shape_renderer(RHI::ITexture* render_target)
		{
			Ref<FillShapeRenderer> renderer = new_object<FillShapeRenderer>();
			lutry
			{
				luexp(renderer->init(render_target));
			}
			lucatchret;
			return renderer;
		}
	}
}