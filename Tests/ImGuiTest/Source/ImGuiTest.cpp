/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file ImGuiTest.cpp
* @author JXMaster
* @date 2020/4/12
*/
#include <ImGui/ImGui.hpp>
#include <Runtime/Runtime.hpp>
#include <RHI/RHI.hpp>
#include <Runtime/Module.hpp>
#include <Runtime/Debug.hpp>
#include <Runtime/Log.hpp>

using namespace Luna;

void on_window_close(Window::IWindow* window)
{
	window->close();
}

void run()
{
	set_log_std_enabled(true);
	using namespace RHI;
	using namespace Window;
	Ref<IWindow> window = new_window("ImGui Demo", WindowDisplaySettings::as_windowed(), WindowCreationFlag::resizable).get();
	window->get_close_event() += on_window_close;

	Ref<IDevice> dev = get_main_device();

	u32 queue = U32_MAX;
	u32 num_queues = dev->get_num_command_queues();
	for (u32 i = 0; i < num_queues; ++i)
	{
		auto desc = dev->get_command_queue_desc(i);
		if (desc.type == RHI::CommandQueueType::graphics)
		{
			queue = i;
			break;
		}
	}

	Ref<ISwapChain> swap_chain = dev->new_swap_chain(queue, window, SwapChainDesc({0, 0, 2, Format::bgra8_unorm, true})).get();

	Ref<ICommandBuffer> cmdbuf = dev->new_command_buffer(queue).get();

	// Create back buffer.
	u32 w = 0, h = 0;

	ImGuiUtils::set_active_window(window);

	while (true)
	{
		poll_events();

		if (window->is_closed())
		{
			break;
		}

		// Recreate the back buffer if needed.
		auto sz = window->get_size();
		auto ww = sz.x;
		auto wh = sz.y;
		if (ww != w || wh != h)
		{
			lupanic_if_failed(swap_chain->reset({ww, wh, 2, Format::unknown, true}));
			f32 clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			w = ww;
			h = wh;
		}

		ImGuiUtils::update_io();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::Render();
		
		auto back_buffer = swap_chain->get_current_back_buffer().get();
		RenderPassDesc desc;
		desc.color_attachments[0] = ColorAttachment(back_buffer, LoadOp::clear, StoreOp::store, { 0.0f, 0.0f, 0.0f, 1.0f });
		cmdbuf->begin_render_pass(desc);
		cmdbuf->end_render_pass();
		ImGuiUtils::render_draw_data(ImGui::GetDrawData(), cmdbuf, back_buffer);
		cmdbuf->resource_barrier({},
		{
			{swap_chain->get_current_back_buffer().get(), SubresourceIndex(0, 0), TextureStateFlag::automatic, TextureStateFlag::present, ResourceBarrierFlag::none}
		});
		cmdbuf->submit({}, {}, true);
		cmdbuf->wait();
		swap_chain->present();
		cmdbuf->reset();
	}
}

int main()
{
	// Start modules.
	Luna::init();
	auto res = Luna::init_modules();
	if (failed(res))
	{
		debug_printf("Module init error: %s\n", explain(res.errcode()));
		lupanic();
		Luna::close();
		return 0;
	}
	run();
	Luna::close();
	return 0;
}