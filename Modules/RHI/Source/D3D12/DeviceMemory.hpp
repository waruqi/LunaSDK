/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
*
* @file DeviceMemory.hpp
* @author JXMaster
* @date 2023/5/16
*/
#pragma once
#include "D3D12Common.hpp"
#include "Device.hpp"

namespace Luna
{
	namespace RHI
	{
		struct DeviceMemory : IDeviceMemory
		{
			lustruct("RHI::DeviceMemory", "{070A7A5C-8C56-4F93-B13A-8E34BCFDAD67}");
			luiimpl();

			Ref<Device> m_device;
			ComPtr<D3D12MA::Allocation> m_allocation;

			RV init(const D3D12MA::ALLOCATION_DESC& allocation_desc, const D3D12_RESOURCE_ALLOCATION_INFO& allocation_info);
		
			virtual IDevice* get_device() override { return m_device; }
			virtual void set_name(const Name& name)
			{
				usize len = utf8_to_utf16_len(name.c_str(), name.size());
				wchar_t* buf = (wchar_t*)alloca(sizeof(wchar_t) * (len + 1));
				utf8_to_utf16((c16*)buf, len + 1, name.c_str(), name.size());
				m_allocation->SetName(buf);
			}
			virtual u64 get_size() override
			{
				return m_allocation->GetSize();
			}
		};
	}
}