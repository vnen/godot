#ifndef APP_UWP_H
#define APP_UWP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>

using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

struct GodotUWPApp : implements<GodotUWPApp, IFrameworkViewSource, IFrameworkView> {
	IFrameworkView CreateView();
	void Initialize(CoreApplicationView const &application_view);
	void Load(hstring const &entry_point);
	void Uninitialize();
	void Run();
	void SetWindow(CoreWindow const &window);
};

#endif
