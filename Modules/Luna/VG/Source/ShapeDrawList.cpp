/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file ShapeDrawList.cpp
* @author JXMaster
* @date 2022/4/17
*/
#include <Luna/Runtime/PlatformDefines.hpp>
#define LUNA_VG_API LUNA_EXPORT
#include "ShapeDrawList.hpp"

namespace Luna
{
	namespace VG
	{
		ShapeDrawCall& ShapeDrawList::get_current_draw_call()
		{
			if (m_state_dirty || m_dc_barrier_index == m_draw_calls.size())
			{
				// If state is changed, we find one available draw call and sets it 
				// to be the current draw call.
				for (u32 i = m_dc_barrier_index; i < m_draw_calls.size(); ++i)
				{
					ShapeDrawCall& dc = m_draw_calls[i];
					if (state_equal(i))
					{
						m_state_dirty = false;
						m_target_dc_index = i;
						return m_draw_calls[m_target_dc_index];
					}
				}
				new_draw_call();
				m_state_dirty = false;
				m_target_dc_index = (i32)m_draw_calls.size() - 1;
				return m_draw_calls[m_target_dc_index];
			}
			else
			{
				return m_draw_calls[m_target_dc_index];
			}
		}
		ShapeDrawCallResource& ShapeDrawList::get_draw_call_resource(usize index)
		{
			while (m_draw_call_resources.size() <= index)
			{
				m_draw_call_resources.emplace_back();
			}
			return m_draw_call_resources[index];
		}
		bool ShapeDrawList::state_equal(u32 index)
		{
			ShapeDrawCall& dc = m_draw_calls[index];
			if (dc.atlas == m_atlas &&
				dc.texture == m_texture &&
				dc.origin_point == m_origin &&
				dc.rotation == m_rotation &&
				dc.clip_rect == m_clip_rect &&
				dc.sampler == m_sampler)
			{
				return true;
			}
			return false;
		}
		void ShapeDrawList::reset()
		{
			lutsassert();
			for (auto& res : m_draw_call_resources)
			{
				res.vertices.clear();
				res.indices.clear();
			}
			m_draw_calls.clear();
			m_atlas = nullptr;
			m_texture = nullptr;
			m_sampler = get_default_sampler();
			m_origin = Float2U(0.0f);
			m_rotation = 0.0f;
			m_clip_rect = RectI(0, 0, 0, 0);
			m_dc_barrier_index = 0;
			m_target_dc_index = 0;
			m_state_dirty = false;
		}
		void ShapeDrawList::append_draw_list(IShapeDrawList* draw_list)
		{
			lutsassert();
			drawcall_barrier();
			ShapeDrawList* rhs = static_cast<ShapeDrawList*>(draw_list->get_object());
			m_draw_calls.reserve(m_draw_calls.size() + rhs->m_draw_calls.size());
			for (usize i = 0; i < rhs->m_draw_calls.size(); ++i)
			{
				m_draw_calls.emplace_back();
				auto& dst = m_draw_calls.back();
				auto& dst_res = get_draw_call_resource(m_draw_calls.size() - 1);
				auto& src = rhs->m_draw_calls[i];
				auto& src_res = rhs->get_draw_call_resource(i);
				dst.atlas = src.atlas;
				dst.texture = src.texture;
				dst.sampler = src.sampler;
				dst.origin_point = src.origin_point;
				dst.rotation = src.rotation;
				dst.clip_rect = src.clip_rect;
				dst_res.vertices = src_res.vertices;
				dst_res.indices = src_res.indices;
			}
			drawcall_barrier();
		}
		void ShapeDrawList::draw_shape_raw(Span<const Vertex> vertices, Span<const u32> indices)
		{
			lutsassert();
			auto& dc = get_current_draw_call();
			auto& dc_res = get_draw_call_resource(m_target_dc_index);
			lucheck_msg(dc.atlas, "Shape atlas must be set before adding draw calls to the shape draw list.");
			dc_res.vertices.insert_n(dc_res.vertices.end(), vertices.data(), vertices.size());
			dc_res.indices.insert_n(dc_res.indices.end(), indices.data(), indices.size());
		}
		void ShapeDrawList::draw_shape(u32 begin_command, u32 num_commands,
			const Float2U& min_position, const Float2U& max_position,
			const Float2U& min_shapecoord, const Float2U& max_shapecoord, u32 color,
			const Float2U& min_texcoord, const Float2U& max_texcoord)
		{
			lutsassert();
			auto& dc = get_current_draw_call();
			auto& dc_res = get_draw_call_resource(m_target_dc_index);
			lucheck_msg(dc.atlas, "Shape atlas must be set before adding draw calls to the shape draw list.");
			u32 idx_offset = (u32)dc_res.vertices.size();
			Vertex v[4];
			v[0].position = min_position;
			//v[1].position = Float2U(min_position.x, max_position.y);
			v[1].position.x = min_position.x;
			v[1].position.y = max_position.y;
			v[2].position = max_position;
			//v[3].position = Float2U(max_position.x, min_position.y);
			v[3].position.x = max_position.x;
			v[3].position.y = min_position.y;
			v[0].shapecoord = min_shapecoord;
			//v[1].shapecoord = Float2U(min_shapecoord.x, max_shapecoord.y);
			v[1].shapecoord.x = min_shapecoord.x;
			v[1].shapecoord.y = max_shapecoord.y;
			v[2].shapecoord = max_shapecoord;
			//v[3].shapecoord = Float2U(max_shapecoord.x, min_shapecoord.y);
			v[3].shapecoord.x = max_shapecoord.x;
			v[3].shapecoord.y = min_shapecoord.y;
			v[0].texcoord = min_texcoord;
			//v[1].texcoord = Float2U(min_texcoord.x, max_texcoord.y);
			v[1].texcoord.x = min_texcoord.x;
			v[1].texcoord.y = max_texcoord.y;
			v[2].texcoord = max_texcoord;
			//v[3].texcoord = Float2U(max_texcoord.x, min_texcoord.y);
			v[3].texcoord.x = max_texcoord.x;
			v[3].texcoord.y = min_texcoord.y;
			v[0].color = v[1].color = v[2].color = v[3].color = color;
			v[0].begin_command = v[1].begin_command = v[2].begin_command = v[3].begin_command = begin_command;
			v[0].num_commands = v[1].num_commands = v[2].num_commands = v[3].num_commands = num_commands;
			dc_res.vertices.insert_n(dc_res.vertices.end(), v, 4);
			u32 indices[] = {
				idx_offset , idx_offset + 1, idx_offset + 2,
				idx_offset , idx_offset + 2, idx_offset + 3 };
			dc_res.indices.insert_n(dc_res.indices.end(), indices, 6);
		}
		RV ShapeDrawList::close()
		{
			lutsassert();
			lutry
			{
				// Pack data.
				u32 num_vertices = 0;
				u32 num_indices = 0;
				for (auto& i : m_draw_call_resources)
				{
					num_vertices += (u32)i.vertices.size();
					num_indices += (u32)i.indices.size();
				}
				if (m_vertex_buffer_capacity < num_vertices)
				{
					// Recreate vertex buffer.
					luset(m_vertex_buffer, RHI::get_main_device()->new_buffer(RHI::MemoryType::upload, RHI::BufferDesc(
						RHI::BufferUsageFlag::vertex_buffer, num_vertices * sizeof(Vertex))));
					m_vertex_buffer_capacity = num_vertices;
				}
				m_vertex_buffer_size = num_vertices;
				if (m_index_buffer_capacity < num_indices)
				{
					// Recreate index buffer.
					luset(m_index_buffer, RHI::get_main_device()->new_buffer(RHI::MemoryType::upload, RHI::BufferDesc(
						RHI::BufferUsageFlag::index_buffer, num_indices * sizeof(u32))));
					m_index_buffer_capacity = num_indices;
				}
				m_index_buffer_size = num_indices;
				Vertex* vertex_data = nullptr;
				luexp(m_vertex_buffer->map(0, 0, (void**)&vertex_data));
				u32* index_data = nullptr;
				luexp(m_index_buffer->map(0, 0, (void**)&index_data));
				u32 vertex_offset = 0;
				u32 index_offset = 0;
				for (usize i = 0; i < m_draw_calls.size(); ++i)
				{
					auto& dc = m_draw_call_resources[i];
					Vertex* dst = vertex_data + vertex_offset;
					memcpy(dst, dc.vertices.data(), dc.vertices.size() * sizeof(Vertex));
					u32* index_dst = index_data + index_offset;
					u32 base_vertex_offset = (u32)vertex_offset;
					for (u32 index : dc.indices)
					{
						*index_dst = index + base_vertex_offset;
						++index_dst;
					}
					m_draw_calls[i].base_index = index_offset;
					m_draw_calls[i].num_indices = (u32)dc.indices.size();
					vertex_offset += (u32)dc.vertices.size();
					index_offset += (u32)dc.indices.size();
				}
				m_vertex_buffer->unmap(0, sizeof(Vertex) * vertex_offset);
				m_index_buffer->unmap(0, sizeof(u32) * index_offset);
			}
			lucatchret;
			return ok;
		}

		LUNA_VG_API Ref<IShapeDrawList> new_shape_draw_list()
		{
			return new_object<ShapeDrawList>();
		}
	}
}