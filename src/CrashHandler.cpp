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
	namespace Modules
	{
		class Factory;
		class Fallout4;
		class Module;

		class Module
		{
		public:
			virtual ~Module() noexcept = default;

			[[nodiscard]] std::uintptr_t address() const noexcept { return reinterpret_cast<std::uintptr_t>(_image.data()); }

			[[nodiscard]] std::string frame_info(const boost::stacktrace::frame& a_frame) const noexcept
			{
				assert(in_range(a_frame.address()));
				return get_frame_info(a_frame);
			}

			[[nodiscard]] bool in_range(const void* a_ptr) const noexcept
			{
				const auto ptr = reinterpret_cast<const std::byte*>(a_ptr);
				return _image.data() <= ptr && ptr < _image.data() + _image.size();
			}

			[[nodiscard]] std::string_view name() const noexcept { return _name; }

		protected:
			friend class Factory;

			Module(std::string a_name, stl::span<const std::byte> a_image) noexcept :
				_name(std::move(a_name)),
				_image(a_image)
			{}

			[[nodiscard]] virtual std::string get_frame_info(const boost::stacktrace::frame& a_frame) const noexcept
			{
				const auto offset = reinterpret_cast<std::uintptr_t>(a_frame.address()) - address();
				return fmt::format(
					FMT_STRING("+{:07X}"),
					offset);
			}

		private:
			std::string _name;
			stl::span<const std::byte> _image;
		};

		class Fallout4 final :
			public Module
		{
		private:
			using super = Module;

		protected:
			friend class Factory;

			using super::super;

			[[nodiscard]] std::string get_frame_info(const boost::stacktrace::frame& a_frame) const noexcept override
			{
				const auto offset = reinterpret_cast<std::uintptr_t>(a_frame.address()) - address();
				const auto it = std::lower_bound(
					_offset2ID.rbegin(),
					_offset2ID.rend(),
					offset,
					[](auto&& a_lhs, auto&& a_rhs) noexcept {
						return a_lhs.offset >= a_rhs;
					});

				auto result = super::get_frame_info(a_frame);
				if (it != _offset2ID.rend()) {
					result += fmt::format(
						FMT_STRING(" -> {}"),
						it->id);
				}
				return result;
			}

		private:
			REL::IDDatabase::Offset2ID _offset2ID{ std::execution::parallel_unsequenced_policy{} };
		};

		class Factory
		{
		public:
			[[nodiscard]] static std::unique_ptr<Module> create(::HMODULE a_module) noexcept
			{
				using result_t = std::unique_ptr<Module>;

				const auto name = get_name(a_module);
				const auto image = get_image(a_module);
				if (_stricmp(name.c_str(), "Fallout4.exe") == 0) {
					return result_t{ new Fallout4(std::move(name), image) };
				} else {
					return result_t{ new Module(std::move(name), image) };
				}
			}

		private:
			[[nodiscard]] static stl::span<const std::byte> get_image(::HMODULE a_module) noexcept
			{
				const auto dosHeader = reinterpret_cast<const ::IMAGE_DOS_HEADER*>(a_module);
				const auto ntHeader = stl::adjust_pointer<::IMAGE_NT_HEADERS64>(dosHeader, dosHeader->e_lfanew);
				return { reinterpret_cast<const std::byte*>(a_module), ntHeader->OptionalHeader.SizeOfImage };
			}

			[[nodiscard]] static std::string get_name(::HMODULE a_module) noexcept
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
				return p.filename().generic_string();
			}
		};

		[[nodiscard]] auto get_loaded_modules() noexcept
			-> std::vector<std::unique_ptr<Module>>
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

			decltype(get_loaded_modules()) results;
			results.resize(modules.size());
			std::for_each(
				std::execution::parallel_unsequenced_policy{},
				modules.begin(),
				modules.end(),
				[&](auto&& a_elem) noexcept {
					const auto pos = std::addressof(a_elem) - modules.data();
					results[pos] = Modules::Factory::create(a_elem);
				});
			std::sort(
				results.begin(),
				results.end(),
				[](auto&& a_lhs, auto&& a_rhs) noexcept {
					return a_lhs->address() < a_rhs->address();
				});

			return results;
		}
	}

	class Callstack
	{
	public:
		using value_type = Modules::Module;
		using const_pointer = const value_type*;

		Callstack(const ::EXCEPTION_RECORD& a_except) noexcept :
			_frames(get_callstack(a_except))
		{}

		void print(
			std::shared_ptr<spdlog::logger> a_log,
			stl::span<const std::unique_ptr<value_type>> a_modules) const noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("CALL STACK:"sv);

			std::vector<const_pointer> moduleStack;
			moduleStack.reserve(_frames.size());
			for (const auto& frame : _frames) {
				const auto it = std::lower_bound(
					a_modules.rbegin(),
					a_modules.rend(),
					reinterpret_cast<std::uintptr_t>(frame.address()),
					[](auto&& a_lhs, auto&& a_rhs) noexcept {
						return a_lhs->address() >= a_rhs;
					});
				if (it != a_modules.rend() && (*it)->in_range(frame.address())) {
					moduleStack.push_back(it->get());
				} else {
					moduleStack.push_back(nullptr);
				}
			}

			const auto format = get_format([&]() {
				std::size_t max = 0;
				std::for_each(
					moduleStack.begin(),
					moduleStack.end(),
					[&](auto&& a_elem) noexcept {
						max = a_elem ? std::max(max, a_elem->name().length()) : max;
					});
				return max;
			}());

			for (std::size_t i = 0; i < _frames.size(); ++i) {
				const auto mod = moduleStack[i];
				a_log->critical(
					format,
					i,
					(mod ? mod->name() : ""sv),
					(mod ? mod->frame_info(_frames[i]) : ""s));
			}
		}

	private:
		[[nodiscard]] static auto get_callstack(const ::EXCEPTION_RECORD& a_except) noexcept
			-> std::vector<boost::stacktrace::frame>
		{
			boost::stacktrace::stacktrace callstack;
			const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(a_except.ExceptionAddress);
			auto it = std::find_if(
				callstack.begin(),
				callstack.end(),
				[&](auto&& a_elem) noexcept {
					return reinterpret_cast<std::uintptr_t>(a_elem.address()) == exceptionAddress;
				});

			if (it == callstack.end()) {
				it = callstack.begin();
			}

			return { it, callstack.end() };
		}

		[[nodiscard]] std::string get_format(std::size_t a_nameWidth) const noexcept
		{
			return "\t[{:>"s +
				   fmt::to_string(
					   fmt::to_string(_frames.size() - 1)
						   .length()) +
				   "}] {:>"s +
				   fmt::to_string(a_nameWidth) +
				   "}{}"s;
		}

		std::vector<boost::stacktrace::frame> _frames;
	};

	[[nodiscard]] std::shared_ptr<spdlog::logger> get_log() noexcept
	{
		auto path = logger::log_directory();
		if (!path) {
			stl::report_and_fail("failed to find standard log directory"sv);
		}

		const auto time = std::time(nullptr);
		std::tm localTime;
		if (gmtime_s(std::addressof(localTime), std::addressof(time)) != 0) {
			stl::report_and_fail("failed to get current time"sv);
		}

		std::stringstream buf;
		buf << "crash-"sv << std::put_time(std::addressof(localTime), "%Y-%m-%d-%H-%M-%S") << ".log"sv;
		*path /= buf.str();

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(path->string(), true);
		const auto log = std::make_shared<spdlog::logger>("crash log"s, std::move(sink));
		log->set_pattern("%v"s);
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::off);

		return log;
	}

	void print_modules(
		std::shared_ptr<spdlog::logger> a_log,
		stl::span<const std::unique_ptr<Modules::Module>> a_modules) noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("MODULES:"sv);

		const auto format = [&]() noexcept {
			const auto width = [&]() noexcept {
				std::size_t max = 0;
				std::for_each(
					a_modules.begin(),
					a_modules.end(),
					[&](auto&& a_elem) {
						max = std::max(max, a_elem->name().length());
					});
				return max;
			}();

			return "\t{:<"s +
				   fmt::to_string(width) +
				   "} 0x{:012X}"s;
		}();

		for (const auto& mod : a_modules) {
			a_log->critical(
				format,
				mod->name(),
				mod->address());
		}
	}

	void print_plugins(std::shared_ptr<spdlog::logger> a_log) noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("PLUGINS:"sv);

		const auto datahandler = RE::TESDataHandler::GetSingleton();
		if (datahandler) {
			const auto& [files, smallfiles] = datahandler->compiledFileCollection;

			const auto fileFormat = [&](auto&& a_files) {
				return "\t[{:>02X}]{:"s +
					   (!a_files.empty() ? "5"s : "1"s) +
					   "}{}"s;
			}(smallfiles);

			for (const auto file : files) {
				a_log->critical(
					fileFormat,
					file->GetCompileIndex(),
					"",
					file->GetFilename());
			}

			for (const auto file : smallfiles) {
				a_log->critical(
					FMT_STRING("\t[FE:{:>03X}] {}"),
					file->GetSmallFileCompileIndex(),
					file->GetFilename());
			}
		}
	}

	void print_registers(
		std::shared_ptr<spdlog::logger> a_log,
		const ::CONTEXT& a_context) noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("REGISTERS:"sv);

		const std::array intRegs{
			std::make_pair("RAX"sv, a_context.Rax),
			std::make_pair("RCX"sv, a_context.Rcx),
			std::make_pair("RDX"sv, a_context.Rdx),
			std::make_pair("RBX"sv, a_context.Rbx),
			std::make_pair("RSP"sv, a_context.Rsp),
			std::make_pair("RBP"sv, a_context.Rbp),
			std::make_pair("RSI"sv, a_context.Rsi),
			std::make_pair("RDI"sv, a_context.Rdi),
			std::make_pair("R8"sv, a_context.R8),
			std::make_pair("R9"sv, a_context.R9),
			std::make_pair("R10"sv, a_context.R10),
			std::make_pair("R11"sv, a_context.R11),
			std::make_pair("R12"sv, a_context.R12),
			std::make_pair("R13"sv, a_context.R13),
			std::make_pair("R14"sv, a_context.R14),
			std::make_pair("R15"sv, a_context.R15),
		};

		for (const auto& [name, reg] : intRegs) {
			a_log->critical(
				FMT_STRING("\t{:<3} 0x{:X}"),
				name,
				reg);
		}

		// TODO
		[[maybe_unused]] const std::array floatRegs{
			std::make_pair("XMM0"sv, a_context.Xmm0),
			std::make_pair("XMM1"sv, a_context.Xmm1),
			std::make_pair("XMM2"sv, a_context.Xmm2),
			std::make_pair("XMM3"sv, a_context.Xmm3),
			std::make_pair("XMM4"sv, a_context.Xmm4),
			std::make_pair("XMM5"sv, a_context.Xmm5),
			std::make_pair("XMM6"sv, a_context.Xmm6),
			std::make_pair("XMM7"sv, a_context.Xmm7),
			std::make_pair("XMM8"sv, a_context.Xmm8),
			std::make_pair("XMM9"sv, a_context.Xmm9),
			std::make_pair("XMM10"sv, a_context.Xmm10),
			std::make_pair("XMM11"sv, a_context.Xmm11),
			std::make_pair("XMM12"sv, a_context.Xmm12),
			std::make_pair("XMM13"sv, a_context.Xmm13),
			std::make_pair("XMM14"sv, a_context.Xmm14),
			std::make_pair("XMM15"sv, a_context.Xmm15),
		};
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
				const auto log = get_log();

				const auto modules = Modules::get_loaded_modules();

				const Callstack callstack{ *a_except->ExceptionRecord };
				callstack.print(log, stl::make_span(modules.begin(), modules.end()));

				log->critical(""sv);
				print_registers(log, *a_except->ContextRecord);
				log->critical(""sv);
				print_modules(log, stl::make_span(modules.begin(), modules.end()));
				log->critical(""sv);
				print_plugins(log);

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
