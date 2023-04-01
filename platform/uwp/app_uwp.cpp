#include "app_uwp.h"

IFrameworkView GodotUWPApp::CreateView() {
	return *this;
}

void GodotUWPApp::Initialize(CoreApplicationView const &) {}

void GodotUWPApp::Load(hstring const &) {}

void GodotUWPApp::Uninitialize() {}

void GodotUWPApp::Run() {
	CoreWindow window = CoreWindow::GetForCurrentThread();
	window.Activate();

	CoreDispatcher dispatcher = window.Dispatcher();
	dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
}

void GodotUWPApp::SetWindow(CoreWindow const &) {}

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
