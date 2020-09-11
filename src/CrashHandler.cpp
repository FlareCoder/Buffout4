#include "CrashHandler.h"

#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
//#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
//#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <Windows.h>

#include <Psapi.h>

#include <boost/stacktrace.hpp>

namespace Crash
{
	class Module
	{
	public:
		Module(::HMODULE a_module) noexcept
		{
			std::vector<char> buf;
			buf.reserve(MAX_PATH);
			buf.resize(MAX_PATH / 2);
			std::uint32_t result = 0;
			do {
				buf.resize(buf.size() * 2);
				result = ::GetModuleFileNameA(
					a_module,
					buf.data(),
					static_cast<std::uint32_t>(buf.size()));
			} while (result && result == buf.size() && buf.size() <= std::numeric_limits<std::uint32_t>::max());
			const std::filesystem::path p = buf.data();
			_name = p.filename().generic_string();

			const auto dosHeader = reinterpret_cast<const ::IMAGE_DOS_HEADER*>(a_module);
			const auto ntHeader = stl::adjust_pointer<::IMAGE_NT_HEADERS64>(dosHeader, dosHeader->e_lfanew);
			_image = stl::span{ reinterpret_cast<const std::byte*>(a_module), ntHeader->OptionalHeader.SizeOfImage };
		}

		[[nodiscard]] std::uintptr_t address() const noexcept { return reinterpret_cast<std::uintptr_t>(_image.data()); }

		[[nodiscard]] bool in_range(const void* a_ptr) const noexcept
		{
			const auto ptr = reinterpret_cast<const std::byte*>(a_ptr);
			return _image.data() <= ptr && ptr < _image.data() + _image.size();
		}

		[[nodiscard]] std::string_view name() const noexcept { return _name; }

	private:
		std::string _name;
		stl::span<const std::byte> _image;
	};

	[[nodiscard]] std::vector<boost::stacktrace::frame> get_callstack(const ::EXCEPTION_RECORD& a_except) noexcept
	{
		boost::stacktrace::stacktrace callstack;
		const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(a_except.ExceptionAddress);
		auto it = std::find_if(
			callstack.begin(),
			callstack.end(),
			[&](auto&& a_elem) {
				return reinterpret_cast<std::uintptr_t>(a_elem.address()) == exceptionAddress;
			});

		if (it == callstack.end()) {
			it = callstack.begin();
		}

		return { it, callstack.end() };
	}

	[[nodiscard]] std::vector<Module> get_loaded_modules() noexcept
	{
		const auto proc = ::GetCurrentProcess();
		std::vector<::HMODULE> modules;
		std::uint32_t needed = 0;
		do {
			modules.resize(needed / sizeof(::HMODULE));
			::K32EnumProcessModules(
				proc,
				modules.data(),
				static_cast<::DWORD>(modules.size() * sizeof(::HMODULE)),
				reinterpret_cast<::DWORD*>(std::addressof(needed)));
		} while ((modules.size() * sizeof(::HMODULE)) < needed);

		std::vector<Module> results;
		std::copy(
			modules.begin(),
			modules.end(),
			std::back_inserter(results));
		std::sort(
			results.begin(),
			results.end(),
			[](auto&& a_lhs, auto&& a_rhs) noexcept {
				return a_lhs.address() < a_rhs.address();
			});

		return results;
	}

	[[nodiscard]] std::shared_ptr<spdlog::logger> get_log() noexcept
	{
		auto path = logger::log_directory();
		if (!path) {
			stl::report_and_fail("failed to find standard log directory"sv);
		}

		*path /= "crash.log"sv;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(path->string(), true);
		const auto log = std::make_shared<spdlog::logger>("crash log"s, std::move(sink));
		log->set_pattern("%v"s);
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::off);

		return log;
	}

	std::int32_t Handler(::EXCEPTION_POINTERS* a_except) noexcept
	{
		switch (a_except->ExceptionRecord->ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		case EXCEPTION_DATATYPE_MISALIGNMENT:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_STACK_OVERFLOW:
			{
				const auto modules = get_loaded_modules();
				const auto callstack = get_callstack(*a_except->ExceptionRecord);

				const auto log = get_log();
				log->critical("CALL STACK:"sv);
				for (std::size_t i = 0; i < callstack.size(); ++i) {
					const auto& frame = callstack[i];
					const auto it = std::lower_bound(
						modules.rbegin(),
						modules.rend(),
						reinterpret_cast<std::uintptr_t>(frame.address()),
						[](auto&& a_lhs, auto&& a_rhs) {
							return a_lhs.address() >= a_rhs;
						});
					const auto name =
						it != modules.rend() && it->in_range(frame.address()) ?
							  it->name() :
							  "<UNKNOWN>"sv;
					log->critical(
						FMT_STRING("\t#{:>2} 0x{:08X} \"{}\""),
						i,
						reinterpret_cast<std::uintptr_t>(frame.address()),
						name);
				}
				log->flush();

				::TerminateProcess(
					::GetCurrentProcess(),
					EXIT_FAILURE);
			}
			break;
		default:
			break;
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}

	void Install()
	{
		CrashToDesktop::Install();
		const auto success =
			::AddVectoredExceptionHandler(
				1,
				reinterpret_cast<::PVECTORED_EXCEPTION_HANDLER>(&Handler));
		if (success == nullptr) {
			stl::report_and_fail("failed to install exception handler"sv);
		}
		logger::info("installed crash handlers"sv);
	}
}
