#ifndef OS_UWP_H
#define OS_UWP_H

#include "core/os/os.h"

#include <winrt/windows.devices.h>
#include <winrt/windows.devices.input.h>
#include <winrt/windows.ui.core.h>

#ifdef XAUDIO2_ENABLED
#include "drivers/xaudio2/audio_driver_xaudio2.h"
#endif

class OS_UWP : public OS {
	friend struct GodotUWPApp;
	friend class DisplayServerUWP;

	uint64_t ticks_start = 0;
	uint64_t ticks_per_second = 0;

	winrt::Windows::UI::Core::CoreWindow core_window = nullptr;
	MainLoop *main_loop = nullptr;

	String cwd;

#ifdef XAUDIO2_ENABLED
	AudioDriverXAudio2 driver_xaudio2;
#endif

protected:
	virtual void initialize() override;
	virtual void initialize_joypads() override;

	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual void delete_main_loop() override;

	virtual void finalize() override;
	virtual void finalize_core() override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;

public:
	virtual Vector<String> get_video_adapter_driver_info() const override;

	virtual String get_stdin_string() override;

	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override; // Should return cryptographically-safe random bytes.

	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override;
	virtual Error kill(const ProcessID &p_pid) override;
	virtual bool is_process_running(const ProcessID &p_pid) const override;

	virtual bool has_environment(const String &p_var) const override;
	virtual String get_environment(const String &p_var) const override;
	virtual void set_environment(const String &p_var, const String &p_value) const override;
	virtual void unset_environment(const String &p_var) const override;

	virtual String get_name() const override;
	virtual String get_distribution_name() const override;
	virtual String get_version() const override;

	virtual MainLoop *get_main_loop() const override;

	virtual DateTime get_datetime(bool utc = false) const override;
	virtual TimeZoneInfo get_time_zone_info() const override;

	virtual void delay_usec(uint32_t p_usec) const override;

	virtual uint64_t get_ticks_usec() const override;

	virtual String OS_UWP::get_user_data_dir() const override;
	virtual Error set_cwd(const String &p_cwd) override;
	String get_cwd() const;

	virtual String get_executable_path() const override;

	void run();

	OS_UWP();
};

#endif
