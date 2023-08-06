/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file PipelineState.hpp
* @author JXMaster
* @date 2019/8/15
*/
#pragma once
#include "Device.hpp"
#include "PipelineLayout.hpp"
#include "../../PipelineState.hpp"
#include "D3D12Common.hpp"

namespace Luna
{
	namespace RHI
	{
		struct PipelineState : IPipelineState
		{
			lustruct("RHI::PipelineState", "{31F529FE-43C4-4DF1-842B-BAF52CCFCF3F}");
			luiimpl()

			Ref<Device> m_device;
			ComPtr<ID3D12PipelineState> m_pso;
			bool m_is_graphics;

			PrimitiveTopology m_primitive_topology;

			PipelineState(Device* dev) :
				m_device(dev),
				m_primitive_topology(PrimitiveTopology::triangle_list) {}

			IDevice* get_device()
			{
				return m_device.as<IDevice>();
			}
			void set_name(const c8* name) { set_object_name(m_pso.Get(), name); }

			bool init_graphic(const GraphicsPipelineStateDesc& desc);
			bool init_compute(const ComputePipelineStateDesc& desc);
		};
	}
}