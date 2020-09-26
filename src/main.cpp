#include "Crash/CrashHandler.h"
#include "Fixes/Fixes.h"
#include "Patches/Patches.h"

bool g_preloaded = false;

void F4SEAPI MessageHandler(F4SE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case F4SE::MessagingInterface::kGameDataReady:
		Fixes::PostInit();
		break;
	}
}

void OpenLog()
{
#ifndef NDEBUG
	auto sink = std::make_shared<logger::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= "Buffout4.log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
}

extern "C" DLLEXPORT int __stdcall DllMain(void*, unsigned long a_reason, void*)
{
#ifndef NDEBUG
	for (; !WinAPI::IsDebuggerPresent();) {}
#endif

	if (a_reason == WinAPI::DLL_PROCESS_ATTACH) {
		OpenLog();
		F4SE::AllocTrampoline(1 << 7);
		Patches::Preload();
		g_preloaded = true;
	}

	return WinAPI::TRUE;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	if (!g_preloaded) {
		stl::report_and_fail("The plugin preloader is not installed or did not run correctly"sv);
	}

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "Buffout4";
	a_info->version = 1;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}"sv, ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	logger::info("Buffout4 loaded"sv);

	F4SE::Init(a_f4se);

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		return false;
	}

	Crash::Install();
	Fixes::PreInit();

	return true;
}
