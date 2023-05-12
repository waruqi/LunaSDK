/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
*
* @file Resource.hpp
* @author JXMaster
* @date 2023/4/19
*/
#pragma once
#include "DeviceMemory.hpp"

namespace Luna
{
	namespace RHI
	{
		struct BufferResource : IBuffer
		{
			lustruct("RHI::BufferResource", "{2CE2F6F7-9CCB-4DD5-848A-DBE27F8A8B7A}");
			luiimpl();

			Ref<Device> m_device;
			Name m_name;

			BufferDesc m_desc;
			VkBuffer m_buffer = VK_NULL_HANDLE;
			Ref<DeviceMemory> m_memory;

			u32 m_owning_queue_family_index = U32_MAX;

			RV post_init();
			RV init_as_committed(const BufferDesc& desc);
			RV init_as_aliasing(const BufferDesc& desc, DeviceMemory* memory);
			~BufferResource();

			virtual IDevice* get_device() override { return m_device.get(); }
			virtual void set_name(const Name& name) override { m_name = name; }
			virtual BufferDesc get_desc() override { return m_desc; }
			virtual R<void*> map(usize read_begin, usize read_end) override;
			virtual void unmap(usize write_begin, usize write_end) override;
		};

		struct ImageGlobalState
		{
			VkImageLayout m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			u32 m_owning_queue_family_index = U32_MAX;
		};

		struct ImageResource : ITexture
		{
			lustruct("RHI::ImageResource", "{731F1D3C-2864-44A4-B380-CF03CBB7AFED}");
			luiimpl();
		
			Ref<Device> m_device;
			Name m_name;

			TextureDesc m_desc;
			VkImage m_image = VK_NULL_HANDLE;
			// This may be `nullptr` if the image memory is managed by external entities, 
			// for example, swap chains. In such case, the image should not be destroyed when
			// the image resource is freed.
			Ref<DeviceMemory> m_memory;

			// Global state.
			Vector<ImageGlobalState> m_global_states;

			RV post_init();
			RV init_as_committed(const TextureDesc& desc);
			RV init_as_aliasing(const TextureDesc& desc, DeviceMemory* memory);
			~ImageResource();

			u32 count_subresources() const
			{
				return m_desc.mip_levels * m_desc.array_size;
			}

			virtual IDevice* get_device() override { return m_device.get(); }
			virtual void set_name(const Name& name) override { m_name = name; }
			virtual TextureDesc get_desc() override { return m_desc; }
		};
	}
}