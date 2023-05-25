#include "os_uwp.h"

#include "core/io/dir_access.h"
#include "drivers/unix/ip_unix.h"
#include "drivers/unix/net_socket_posix.h"
#include "drivers/windows/dir_access_windows.h"
#include "drivers/windows/file_access_windows.h"
#include "main/main.h"
#include "platform/uwp/display_server_uwp.h"
#include "servers/audio_server.h"

#include <bcrypt.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <winrt/windows.applicationmodel.core.h>
#include <winrt/windows.storage.h>

void OS_UWP::initialize() {
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_FILESYSTEM);

	NetSocketPosix::make_default();

	QueryPerformanceFrequency((LARGE_INTEGER *)&ticks_per_second);
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks_start);

	IPUnix::make_default();
}

void OS_UWP::initialize_joypads() {}

void OS_UWP::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_UWP::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_UWP::finalize() {
	delete_main_loop();
}

void OS_UWP::finalize_core() {}

bool OS_UWP::_check_internal_feature_support(const String &p_feature) {
	return false;
}

Vector<String> OS_UWP::get_video_adapter_driver_info() const {
	return Vector<String>();
}

String OS_UWP::get_stdin_string() {
	return String();
}

Error OS_UWP::get_entropy(uint8_t *r_buffer, int p_bytes) {
	NTSTATUS status = BCryptGenRandom(nullptr, r_buffer, p_bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	ERR_FAIL_COND_V(status, FAILED);
	return OK;
}

Error OS_UWP::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_UWP::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_UWP::kill(const ProcessID &p_pid) {
	return ERR_UNAVAILABLE;
}

bool OS_UWP::is_process_running(const ProcessID &p_pid) const {
	return false;
}

bool OS_UWP::has_environment(const String &p_var) const {
	return false;
}

String OS_UWP::get_environment(const String &p_var) const {
	return String();
}

void OS_UWP::set_environment(const String &p_var, const String &p_value) const {}

void OS_UWP::unset_environment(const String &p_var) const {}

String OS_UWP::get_name() const {
	return String();
}

String OS_UWP::get_distribution_name() const {
	return String();
}

String OS_UWP::get_version() const {
	return String();
}

MainLoop *OS_UWP::get_main_loop() const {
	return main_loop;
}

OS::DateTime OS_UWP::get_datetime(bool utc) const {
	return DateTime();
}

OS::TimeZoneInfo OS_UWP::get_time_zone_info() const {
	return TimeZoneInfo();
}

void OS_UWP::delay_usec(uint32_t p_usec) const {
	int msec = p_usec < 1000 ? 1 : p_usec / 1000;

	WaitForSingleObjectEx(GetCurrentThread(), msec, false);
}

uint64_t OS_UWP::get_ticks_usec() const {
	uint64_t ticks;

	// This is the number of clock ticks since start
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks);
	// Subtract the ticks at game start to get
	// the ticks since the game started
	ticks -= ticks_start;

	// Divide by frequency to get the time in seconds
	// original calculation shown below is subject to overflow
	// with high ticks_per_second and a number of days since the last reboot.
	// time = ticks * 1000000L / ticks_per_second;

	// we can prevent this by either using 128 bit math
	// or separating into a calculation for seconds, and the fraction
	uint64_t seconds = ticks / ticks_per_second;

	// compiler will optimize these two into one divide
	uint64_t leftover = ticks % ticks_per_second;

	// remainder
	uint64_t time = (leftover * 1000000L) / ticks_per_second;

	// seconds
	time += seconds * 1000000L;

	return time;
}

String OS_UWP::get_user_data_dir() const {
	using namespace winrt::Windows::Storage;
	StorageFolder data_folder = ApplicationData::Current().LocalFolder();
	return String(data_folder.Path().data()).replace("\\", "/");
}

Error OS_UWP::set_cwd(const String &p_cwd) {
	if (DirAccess::exists(p_cwd)) {
		cwd = p_cwd;
		return OK;
	}
	return ERR_CANT_OPEN;
}

String OS_UWP::get_cwd() const {
	return cwd;
}

String OS_UWP::get_executable_path() const {
	using namespace winrt::Windows::Storage;
	using namespace winrt::Windows::ApplicationModel;
	StorageFolder install_folder = Package::Current().InstalledLocation();
	return String(install_folder.Path().data());
}

void OS_UWP::run() {
	if (!main_loop) {
		return;
	}

	main_loop->initialize();

	while (true) {
		DisplayServer::get_singleton()->process_events(); // get rid of pending events
		if (Main::iteration()) {
			break;
		}
	}

	main_loop->finalize();
}

OS_UWP::OS_UWP() {
#ifdef XAUDIO2_ENABLED
	AudioDriverManager::add_driver(&driver_xaudio2);
#endif

	DisplayServerUWP::register_uwp_driver();

	cwd = get_executable_path();
}
