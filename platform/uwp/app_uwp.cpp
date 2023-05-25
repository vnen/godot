#include "app_uwp.h"

#include "main/main.h"
#include "os_uwp.h"

#include <winrt/windows.applicationmodel.core.h>

IFrameworkView GodotUWPApp::CreateView() {
	return *this;
}

void GodotUWPApp::Initialize(CoreApplicationView const &p_application_view) {
	os = new OS_UWP();

	p_application_view.Activated({ this, &GodotUWPApp::OnActivated });
}

void GodotUWPApp::Load(hstring const &) {}

void GodotUWPApp::Uninitialize() {
	Main::cleanup();
	delete os;
}

void GodotUWPApp::Run() {
	if (Main::start()) {
		os->run();
	}
}

void GodotUWPApp::SetWindow(CoreWindow const &p_window) {
	uwp_window = p_window;
	os->core_window = p_window;

	// static char *fail_cl[] = { "--verbose", "--main-pack", "basic.pck", nullptr };
	static char *fail_cl[] = { "--gpu-validation", "--verbose", "--path", "game", nullptr };
	Main::setup(".", 4, fail_cl, false);

	Main::setup2();
}

void GodotUWPApp::OnActivated(CoreApplicationView const & /*p_application_view*/, IActivatedEventArgs const & /*p_args*/) {
	CoreWindow::GetForCurrentThread().Activate();
}

// For SUBSYSTEM:WINDOWS
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	CoreApplication::Run(make<GodotUWPApp>());
	return 0;
}

// For SUBSYSTEM:CONSOLE
int main() {
	CoreApplication::Run(make<GodotUWPApp>());
	return 0;
}
