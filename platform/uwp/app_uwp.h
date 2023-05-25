#ifndef APP_UWP_H
#define APP_UWP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winrt/windows.applicationmodel.activation.h>
#include <winrt/windows.applicationmodel.core.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.ui.core.h>

using namespace winrt;

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

class OS_UWP;

struct GodotUWPApp : implements<GodotUWPApp, IFrameworkViewSource, IFrameworkView> {
private:
	OS_UWP *os = nullptr;
	CoreWindow uwp_window = nullptr;

public:
	IFrameworkView CreateView();
	void Initialize(CoreApplicationView const &p_application_view);
	void Load(hstring const &p_entry_point);
	void Uninitialize();
	void Run();
	void SetWindow(CoreWindow const &p_window);

	void OnActivated(CoreApplicationView const &p_application_view, IActivatedEventArgs const &p_args);
};

#endif
