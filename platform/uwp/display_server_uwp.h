#ifndef DISPLAY_SERVER_UWP_H
#define DISPLAY_SERVER_UWP_H

#include "platform/uwp/joypad_uwp.h"
#include "servers/display_server.h"
#include "servers/display_server_headless.h"

#ifdef XAUDIO2_ENABLED
#include "drivers/xaudio2/audio_driver_xaudio2.h"
#endif

#if defined(D3D12_ENABLED)
#include "drivers/d3d12/rendering_device_d3d12.h"
#endif

#include <winrt/windows.devices.input.h>
#include <winrt/windows.ui.core.h>

class OS_UWP;

class DisplayServerUWP : public DisplayServer {
#if defined(D3D12_ENABLED)
	D3D12Context *context_d3d12 = nullptr;
	RenderingDeviceD3D12 *rendering_device_d3d12 = nullptr;
#endif

	String rendering_driver;

	friend class OS_UWP;
	winrt::Windows::UI::Core::CoreWindow core_window = nullptr;
	JoypadUWP *joypad = nullptr;

	MouseMode mouse_mode = MOUSE_MODE_VISIBLE;
	winrt::event_token mouse_moved_token;
	BitField<MouseButtonMask> mouse_state;
	int last_touch_x[32]; // 20 fingers, index 31 reserved for the mouse
	int last_touch_y[32];
	winrt::Windows::Foundation::Point last_mouse_pos;

	void pointer_event(const winrt::Windows::UI::Core::CoreWindow &sender, const winrt::Windows::UI::Core::PointerEventArgs &args, bool p_pressed, bool p_is_wheel = false);
	void on_pointer_pressed(const winrt::Windows::UI::Core::CoreWindow &sender, const winrt::Windows::UI::Core::PointerEventArgs &args);
	void on_pointer_released(const winrt::Windows::UI::Core::CoreWindow &sender, const winrt::Windows::UI::Core::PointerEventArgs &args);
	void on_pointer_moved(const winrt::Windows::UI::Core::CoreWindow &sender, const winrt::Windows::UI::Core::PointerEventArgs &args);
	void on_mouse_moved(const winrt::Windows::Devices::Input::MouseDevice &mouse_device, const winrt::Windows::Devices::Input::MouseEventArgs &args);
	void on_pointer_wheel_changed(const winrt::Windows::UI::Core::CoreWindow &sender, const winrt::Windows::UI::Core::PointerEventArgs &args);
	void on_mouse_mode_changed();

public:
	virtual bool has_feature(Feature p_feature) const override;
	virtual String get_name() const override;

	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;

	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;

	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual int get_screen_from_rect(const Rect2 &p_rect) const;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;

	virtual Vector<DisplayServer::WindowID> get_window_list() const override;

	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_transient(WindowID p_window, WindowID p_parent) override;

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override;

	virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool can_any_window_draw() const override;

	virtual void process_events() override;

	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
	static Vector<String> get_rendering_drivers_func();

	static void register_uwp_driver();

	DisplayServerUWP(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
	~DisplayServerUWP();
};

#endif
