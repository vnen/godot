#include "display_server_uwp.h"

#include "core/input/input.h"
#include "core/os/memory.h"
#include "os_uwp.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"

#include <winrt/base.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.graphics.display.h>
#include <winrt/windows.ui.core.h>
#include <winrt/windows.ui.input.h>

using namespace winrt::Windows::Devices;
using namespace winrt::Windows::Devices::Input;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input;

static inline float ConvertDipsToPixels(float dips, float dpi) {
	static const float dipsPerInch = 96.0f;
	return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
}

bool DisplayServerUWP::has_feature(Feature p_feature) const {
	switch (p_feature) {
		case FEATURE_TOUCHSCREEN:
		case FEATURE_MOUSE:
		// case FEATURE_MOUSE_WARP:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_IME:
		case FEATURE_ICON:
		case FEATURE_NATIVE_ICON:
		case FEATURE_SWAP_BUFFERS:
		case FEATURE_CLIPBOARD_PRIMARY:
			return true;
		default:
			return false;
	}
}

String DisplayServerUWP::get_name() const {
	return "UWP";
}

void DisplayServerUWP::mouse_set_mode(MouseMode p_mode) {
	if (mouse_mode == p_mode) {
		// Already in the same mode; do nothing.
		return;
	}

	if (p_mode == MouseMode::MOUSE_MODE_CAPTURED) {
		CoreWindow::GetForCurrentThread().SetPointerCapture();
	} else {
		CoreWindow::GetForCurrentThread().ReleasePointerCapture();
	}

	if (p_mode == MouseMode::MOUSE_MODE_HIDDEN || p_mode == MouseMode::MOUSE_MODE_CAPTURED || p_mode == MouseMode::MOUSE_MODE_CONFINED_HIDDEN) {
		CoreWindow::GetForCurrentThread().PointerCursor(nullptr);
	} else {
		CoreWindow::GetForCurrentThread().PointerCursor(CoreCursor(CoreCursorType::Arrow, 0));
	}

	mouse_mode = p_mode;

	on_mouse_mode_changed();
}

DisplayServer::MouseMode DisplayServerUWP::mouse_get_mode() const {
	return mouse_mode;
}

Point2i DisplayServerUWP::mouse_get_position() const {
	return Point2i(last_mouse_pos.X, last_mouse_pos.Y);
}

BitField<MouseButtonMask> DisplayServerUWP::mouse_get_button_state() const {
	return mouse_state;
}

int DisplayServerUWP::get_screen_count() const {
	return 1;
}

int DisplayServerUWP::get_primary_screen() const {
	return 0;
}

int DisplayServerUWP::get_screen_from_rect(const Rect2 &p_rect) const {
	return 0;
}

Point2i DisplayServerUWP::screen_get_position(int p_screen) const {
	using namespace winrt::Windows::Graphics::Display;
	DisplayInformation info = DisplayInformation::GetForCurrentView();
	return Point2i(ConvertDipsToPixels(core_window.Bounds().X, info.LogicalDpi()), ConvertDipsToPixels(core_window.Bounds().Y, info.LogicalDpi()));
}

Size2i DisplayServerUWP::screen_get_size(int p_screen) const {
	using namespace winrt::Windows::Graphics::Display;
	DisplayInformation info = DisplayInformation::GetForCurrentView();
	return Size2i(info.ScreenWidthInRawPixels(), info.ScreenHeightInRawPixels());
}

Rect2i DisplayServerUWP::screen_get_usable_rect(int p_screen) const {
	Size2i size = screen_get_size();
	return Rect2i(Point2i(0, 0), size);
}

int DisplayServerUWP::screen_get_dpi(int p_screen) const {
	using namespace winrt::Windows::Graphics::Display;
	DisplayInformation info = DisplayInformation::GetForCurrentView();
	return info.LogicalDpi();
}

float DisplayServerUWP::screen_get_refresh_rate(int p_screen) const {
	return 0.0;
}

Vector<DisplayServer::WindowID> DisplayServerUWP::get_window_list() const {
	Vector<DisplayServer::WindowID> list;
	list.push_back(MAIN_WINDOW_ID);
	return list;
}

DisplayServer::WindowID DisplayServerUWP::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

void DisplayServerUWP::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {}

ObjectID DisplayServerUWP::window_get_attached_instance_id(WindowID p_window) const {
	return ObjectID();
}

void DisplayServerUWP::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {}

void DisplayServerUWP::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {}

void DisplayServerUWP::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {}

void DisplayServerUWP::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {}

void DisplayServerUWP::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {}

void DisplayServerUWP::window_set_title(const String &p_title, WindowID p_window) {}

int DisplayServerUWP::window_get_current_screen(WindowID p_window) const {
	return 0;
}

void DisplayServerUWP::window_set_current_screen(int p_screen, WindowID p_window) {}

Point2i DisplayServerUWP::window_get_position(WindowID p_window) const {
	return Point2i(core_window.Bounds().X, core_window.Bounds().Y);
}

Point2i DisplayServerUWP::window_get_position_with_decorations(WindowID p_window) const {
	return Point2i(core_window.Bounds().X, core_window.Bounds().Y);
}

void DisplayServerUWP::window_set_position(const Point2i &p_position, WindowID p_window) {}

void DisplayServerUWP::window_set_transient(WindowID p_window, WindowID p_parent) {}

void DisplayServerUWP::window_set_max_size(const Size2i p_size, WindowID p_window) {}

Size2i DisplayServerUWP::window_get_max_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerUWP::window_set_min_size(const Size2i p_size, WindowID p_window) {}

Size2i DisplayServerUWP::window_get_min_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerUWP::window_set_size(const Size2i p_size, WindowID p_window) {
#if defined(D3D12_ENABLED)
	if (context_d3d12) {
		context_d3d12->window_resize(p_window, p_size.width, p_size.height);
	}
#endif
}

Size2i DisplayServerUWP::window_get_size(WindowID p_window) const {
	using namespace winrt::Windows::Graphics::Display;
	DisplayInformation info = DisplayInformation::GetForCurrentView();
	return Size2i(ConvertDipsToPixels(core_window.Bounds().Width, info.LogicalDpi()), ConvertDipsToPixels(core_window.Bounds().Height, info.LogicalDpi()));
}

Size2i DisplayServerUWP::window_get_size_with_decorations(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerUWP::window_set_mode(WindowMode p_mode, WindowID p_window) {}

DisplayServer::WindowMode DisplayServerUWP::window_get_mode(WindowID p_window) const {
	return WINDOW_MODE_WINDOWED;
}

bool DisplayServerUWP::window_is_maximize_allowed(WindowID p_window) const {
	return false;
}

void DisplayServerUWP::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {}

bool DisplayServerUWP::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return false;
}

void DisplayServerUWP::window_request_attention(WindowID p_window) {}

void DisplayServerUWP::window_move_to_foreground(WindowID p_window) {}

bool DisplayServerUWP::window_can_draw(WindowID p_window) const {
	return true;
}

bool DisplayServerUWP::can_any_window_draw() const {
	return true;
}

void DisplayServerUWP::process_events() {
	using namespace winrt::Windows::UI::Core;
	core_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
	joypad->process_controllers();

	::Input::get_singleton()->flush_buffered_events();
}

Vector<String> DisplayServerUWP::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef D3D12_ENABLED
	drivers.push_back("d3d12");
#endif
	return drivers;
}

DisplayServer *DisplayServerUWP::create_func(const String &p_rendering_driver, DisplayServer::WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerUWP(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, r_error));
	print_line("UWP being created");
	return ds;
}

static MouseButton _get_button(const PointerPoint &pt) {
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
	return MouseButton::LEFT;
#else
	switch (pt.Properties().PointerUpdateKind()) {
		case PointerUpdateKind::LeftButtonPressed:
		case PointerUpdateKind::LeftButtonReleased:
			return MouseButton::LEFT;

		case PointerUpdateKind::RightButtonPressed:
		case PointerUpdateKind::RightButtonReleased:
			return MouseButton::RIGHT;

		case PointerUpdateKind::MiddleButtonPressed:
		case PointerUpdateKind::MiddleButtonReleased:
			return MouseButton::MIDDLE;

		case PointerUpdateKind::XButton1Pressed:
		case PointerUpdateKind::XButton1Released:
			return MouseButton::WHEEL_UP;

		case PointerUpdateKind::XButton2Pressed:
		case PointerUpdateKind::XButton2Released:
			return MouseButton::WHEEL_DOWN;

		default:
			break;
	}
#endif

	return MouseButton::NONE;
}

static bool _is_touch(const PointerPoint &p_pointer_point) {
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
	return true;
#else
	switch (p_pointer_point.PointerDevice().PointerDeviceType()) {
		case PointerDeviceType::Touch:
		case PointerDeviceType::Pen:
			return true;
		default:
			return false;
	}
#endif
}

static Point _get_pixel_position(const CoreWindow &window, Point rawPosition) {
	Point outputPosition;

// Compute coordinates normalized from 0..1.
// If the coordinates need to be sized to the SDL window,
// we'll do that after.
#if 1 || WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
	outputPosition.X = rawPosition.X / window.Bounds().Width;
	outputPosition.Y = rawPosition.Y / window.Bounds().Height;
#else
	switch (DisplayProperties::CurrentOrientation) {
		case DisplayOrientations::Portrait:
			outputPosition.X = rawPosition.X / window.Bounds().Width;
			outputPosition.Y = rawPosition.Y / window.Bounds().Height;
			break;
		case DisplayOrientations::PortraitFlipped:
			outputPosition.X = 1.0f - (rawPosition.X / window.Bounds().Width);
			outputPosition.Y = 1.0f - (rawPosition.Y / window.Bounds().Height);
			break;
		case DisplayOrientations::Landscape:
			outputPosition.X = rawPosition.Y / window.Bounds().Height;
			outputPosition.Y = 1.0f - (rawPosition.X / window.Bounds().Width);
			break;
		case DisplayOrientations::LandscapeFlipped:
			outputPosition.X = 1.0f - (rawPosition.Y / window.Bounds().Height);
			outputPosition.Y = rawPosition.X / window.Bounds().Width;
			break;
		default:
			break;
	}
#endif

	Size2i screen_size = DisplayServer::get_singleton()->window_get_size();
	outputPosition.X *= screen_size.width;
	outputPosition.Y *= screen_size.height;

	// return outputPosition;
	return rawPosition;
}

static int _get_finger(uint32_t p_touch_id) {
	return p_touch_id % 31; // for now
}

void DisplayServerUWP::pointer_event(const CoreWindow &sender, const PointerEventArgs &args, bool p_pressed, bool p_is_wheel) {
	PointerPoint point = args.CurrentPoint();
	Point pos = _get_pixel_position(sender, point.Position());
	MouseButton but = _get_button(point);
	if (_is_touch(point)) {
		Ref<InputEventScreenTouch> screen_touch;
		screen_touch.instantiate();
		screen_touch->set_device(0);
		screen_touch->set_pressed(p_pressed);
		screen_touch->set_position(Vector2(pos.X, pos.Y));
		screen_touch->set_index(_get_finger(point.PointerId()));

		last_touch_x[screen_touch->get_index()] = pos.X;
		last_touch_y[screen_touch->get_index()] = pos.Y;

		::Input::get_singleton()->parse_input_event(screen_touch);
	} else {
		Ref<InputEventMouseButton> mouse_button;
		mouse_button.instantiate();
		mouse_button->set_device(0);
		mouse_button->set_pressed(p_pressed);
		mouse_button->set_button_index(but);
		mouse_button->set_position(Vector2(pos.X, pos.Y));
		mouse_button->set_global_position(Vector2(pos.X, pos.Y));

		if (p_is_wheel) {
			if (point.Properties().MouseWheelDelta() > 0) {
				mouse_button->set_button_index(point.Properties().IsHorizontalMouseWheel() ? MouseButton::WHEEL_RIGHT : MouseButton::WHEEL_UP);
			} else if (point.Properties().MouseWheelDelta() < 0) {
				mouse_button->set_button_index(point.Properties().IsHorizontalMouseWheel() ? MouseButton::WHEEL_LEFT : MouseButton::WHEEL_DOWN);
			}
		}

		last_touch_x[31] = pos.X;
		last_touch_y[31] = pos.Y;

		::Input::get_singleton()->parse_input_event(mouse_button);

		if (p_is_wheel) {
			// Send release for mouse wheel
			mouse_button->set_pressed(false);
			::Input::get_singleton()->parse_input_event(mouse_button);
		} else {
			if (mouse_button->is_pressed()) {
				mouse_state.set_flag(mouse_button_to_mask(but));
			} else {
				mouse_state.clear_flag(mouse_button_to_mask(but));
			}
		}
	}
}

void DisplayServerUWP::on_pointer_pressed(const CoreWindow &sender, const PointerEventArgs &args) {
	pointer_event(sender, args, true);
}

void DisplayServerUWP::on_pointer_released(const CoreWindow &sender, const PointerEventArgs &args) {
	pointer_event(sender, args, false);
}

void DisplayServerUWP::on_pointer_moved(const CoreWindow &sender, const PointerEventArgs &args) {
	PointerPoint point = args.CurrentPoint();
	Point pos = _get_pixel_position(sender, point.Position());

	if (_is_touch(point)) {
		Ref<InputEventScreenDrag> screen_drag;
		screen_drag.instantiate();
		screen_drag->set_device(0);
		screen_drag->set_position(Vector2(pos.X, pos.Y));
		screen_drag->set_index(_get_finger(point.PointerId()));
		screen_drag->set_relative(Vector2(screen_drag->get_position().x - last_touch_x[screen_drag->get_index()], screen_drag->get_position().y - last_touch_y[screen_drag->get_index()]));

		::Input::get_singleton()->parse_input_event(screen_drag);
	} else {
		// In case the mouse grabbed, MouseMoved will handle this
		if (mouse_mode == MOUSE_MODE_CAPTURED) {
			return;
		}

		Ref<InputEventMouseMotion> mouse_motion;
		mouse_motion.instantiate();
		mouse_motion->set_device(0);
		mouse_motion->set_position(Vector2(pos.X, pos.Y));
		mouse_motion->set_global_position(Vector2(pos.X, pos.Y));
		mouse_motion->set_relative(Vector2(pos.X - last_touch_x[31], pos.Y - last_touch_y[31]));

		last_mouse_pos = pos;

		::Input::get_singleton()->parse_input_event(mouse_motion);
	}
}

void DisplayServerUWP::on_mouse_mode_changed() {
	MouseMode mode = mouse_mode;

	core_window.Dispatcher().RunAsync(
			CoreDispatcherPriority::High,
			DispatchedHandler(
					[mode, this]() {
						if (mode == MOUSE_MODE_CAPTURED) {
							this->mouse_moved_token = MouseDevice::GetForCurrentView().MouseMoved(
									{ this, &DisplayServerUWP::on_mouse_moved });

						} else {
							MouseDevice::GetForCurrentView().MouseMoved(this->mouse_moved_token);
						}
					}));
}

void DisplayServerUWP::on_mouse_moved(const MouseDevice &mouse_device, const MouseEventArgs &args) {
	// In case the mouse isn't grabbed, PointerMoved will handle this
	if (mouse_mode != MOUSE_MODE_CAPTURED) {
		return;
	}

	Point pos;
	pos.X = last_mouse_pos.X + args.MouseDelta().X;
	pos.Y = last_mouse_pos.Y + args.MouseDelta().Y;

	Ref<InputEventMouseMotion> mouse_motion;
	mouse_motion.instantiate();
	mouse_motion->set_device(0);
	mouse_motion->set_position(Vector2(pos.X, pos.Y));
	mouse_motion->set_global_position(Vector2(pos.X, pos.Y));
	mouse_motion->set_relative(Vector2(args.MouseDelta().X, args.MouseDelta().Y));

	last_mouse_pos = pos;

	::Input::get_singleton()->parse_input_event(mouse_motion);
}

void DisplayServerUWP::on_pointer_wheel_changed(const CoreWindow &sender, const PointerEventArgs &args) {
	pointer_event(sender, args, true, true);
}

void DisplayServerUWP::register_uwp_driver() {
	register_create_function("uwp", create_func, get_rendering_drivers_func);
}

DisplayServerUWP::DisplayServerUWP(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error) {
	using namespace winrt::Windows::UI::Core;
	rendering_driver = p_rendering_driver;
	core_window = static_cast<OS_UWP *>(OS::get_singleton())->core_window;

#if defined(D3D12_ENABLED)
	print_line("d3d12 enable, rendering driver: " + rendering_driver);
	if (rendering_driver == "d3d12") {
		context_d3d12 = memnew(D3D12Context);
		if (context_d3d12->initialize() != OK) {
			memdelete(context_d3d12);
			context_d3d12 = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
	}
#endif

#ifdef D3D12_ENABLED
	if (context_d3d12) {
		if (context_d3d12->window_create(MAIN_WINDOW_ID, p_vsync_mode, core_window, nullptr, p_resolution.x, p_resolution.y) != OK) {
			memdelete(context_d3d12);
			context_d3d12 = nullptr;
			ERR_FAIL_MSG("Failed to create D3D12 Window.");
		}
	}
#endif

#if defined(D3D12_ENABLED)
	if (rendering_driver == "d3d12") {
		rendering_device_d3d12 = memnew(RenderingDeviceD3D12);
		rendering_device_d3d12->initialize(context_d3d12);

		RendererCompositorRD::make_current();
	}
#endif

	joypad = memnew(JoypadUWP);
	joypad->register_events();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	// Disable all pointer visual feedback for better performance when touching.
	// This is only supported in Desktop applications.
	PointerVisualizationSettings pointerVisualizationSettings = PointerVisualizationSettings::GetForCurrentView();
	pointerVisualizationSettings.IsContactFeedbackEnabled(false);
	pointerVisualizationSettings.IsBarrelButtonFeedbackEnabled(false);
#endif

	core_window.PointerPressed({ this, &DisplayServerUWP::on_pointer_pressed });
	core_window.PointerMoved({ this, &DisplayServerUWP::on_pointer_moved });
	core_window.PointerReleased({ this, &DisplayServerUWP::on_pointer_released });
	core_window.PointerWheelChanged({ this, &DisplayServerUWP::on_pointer_wheel_changed });

	r_error = OK;
}

DisplayServerUWP::~DisplayServerUWP() {
	memdelete(joypad);
}
