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
#include <dbghelp.h>
#include <winternl.h>

#include <boost/stacktrace.hpp>

namespace Crash
{
	namespace Modules
	{
		namespace detail
		{
			class VTable
			{
			public:
				VTable(
					std::string_view a_name,
					stl::span<const std::byte> a_module,
					stl::span<const std::byte> a_data,
					stl::span<const std::byte> a_rdata) noexcept
				{
					const auto typeDesc = type_descriptor(a_name, a_data);
					const auto col = typeDesc ? complete_object_locator(typeDesc, a_module, a_rdata) : nullptr;
					_vtable = col ? virtual_table(col, a_rdata) : nullptr;
				}

				[[nodiscard]] const void* get() const noexcept { return _vtable; }

			private:
				[[nodiscard]] auto type_descriptor(
					std::string_view a_name,
					stl::span<const std::byte> a_data) const noexcept
					-> const RE::RTTI::TypeDescriptor*
				{
					constexpr std::size_t offset = 0x10;  // offset of name into type descriptor
					boost::algorithm::knuth_morris_pratt search(a_name.cbegin(), a_name.cend());
					const auto& [first, last] = search(
						reinterpret_cast<const char*>(a_data.data()),
						reinterpret_cast<const char*>(a_data.data() + a_data.size()));
					return first != last ?
								 reinterpret_cast<const RE::RTTI::TypeDescriptor*>(first - offset) :
								 nullptr;
				}

				[[nodiscard]] auto complete_object_locator(
					const RE::RTTI::TypeDescriptor* a_typeDesc,
					stl::span<const std::byte> a_module,
					stl::span<const std::byte> a_rdata) const noexcept
					-> const RE::RTTI::CompleteObjectLocator*
				{
					assert(a_typeDesc != nullptr);

					const auto typeDesc = reinterpret_cast<std::uintptr_t>(a_typeDesc);
					const auto rva = static_cast<std::uint32_t>(typeDesc - reinterpret_cast<std::uintptr_t>(a_module.data()));

					const auto offset = static_cast<std::size_t>(a_rdata.data() - a_module.data());
					const auto base = a_rdata.data();
					const auto start = reinterpret_cast<const std::uint32_t*>(base);
					const auto end = reinterpret_cast<const std::uint32_t*>(base + a_rdata.size());

					for (auto iter = start; iter < end; ++iter) {
						if (*iter == rva) {
							// both base class desc and col can point to the type desc so we check
							// the next int to see if it can be an rva to decide which type it is
							if ((iter[1] < offset) || (offset + a_rdata.size() <= iter[1])) {
								continue;
							}

							const auto ptr = reinterpret_cast<const std::byte*>(iter);
							const auto col = reinterpret_cast<const RE::RTTI::CompleteObjectLocator*>(ptr - offsetof(RE::RTTI::CompleteObjectLocator, typeDescriptor));
							if (col->offset != 0) {
								continue;
							}

							return col;
						}
					}

					return nullptr;
				}

				[[nodiscard]] const void* virtual_table(
					const RE::RTTI::CompleteObjectLocator* a_col,
					stl::span<const std::byte> a_rdata) const noexcept
				{
					assert(a_col != nullptr);

					const auto col = reinterpret_cast<std::uintptr_t>(a_col);

					const auto base = a_rdata.data();
					const auto start = reinterpret_cast<const std::uintptr_t*>(base);
					const auto end = reinterpret_cast<const std::uintptr_t*>(base + a_rdata.size());

					for (auto iter = start; iter < end; ++iter) {
						if (*iter == col) {
							return iter + 1;
						}
					}

					return nullptr;
				}

				const void* _vtable{ nullptr };
			};
		}

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

			[[nodiscard]] bool in_data_range(const void* a_ptr) const noexcept
			{
				const auto ptr = reinterpret_cast<const std::byte*>(a_ptr);
				return _data.data() <= ptr && ptr < _data.data() + _data.size();
			}

			[[nodiscard]] bool in_rdata_range(const void* a_ptr) const noexcept
			{
				const auto ptr = reinterpret_cast<const std::byte*>(a_ptr);
				return _rdata.data() <= ptr && ptr < _rdata.data() + _rdata.size();
			}

			[[nodiscard]] std::string_view name() const noexcept { return _name; }

			[[nodiscard]] const RE::msvc::type_info* type_info() const noexcept { return _typeInfo; }

		protected:
			friend class Factory;

			Module(std::string a_name, stl::span<const std::byte> a_image) noexcept :
				_name(std::move(a_name)),
				_image(a_image)
			{
				auto dosHeader = reinterpret_cast<const ::IMAGE_DOS_HEADER*>(_image.data());
				auto ntHeader = stl::adjust_pointer<::IMAGE_NT_HEADERS64>(dosHeader, dosHeader->e_lfanew);
				stl::span sections(
					IMAGE_FIRST_SECTION(ntHeader),
					ntHeader->FileHeader.NumberOfSections);

				const std::array todo{
					std::make_pair(".data"sv, std::ref(_data)),
					std::make_pair(".rdata"sv, std::ref(_rdata)),
				};
				for (auto& [name, section] : todo) {
					const auto it = std::find_if(
						sections.begin(),
						sections.end(),
						[&](auto&& a_elem) {
							constexpr auto size = std::extent_v<decltype(a_elem.Name)>;
							const auto len = std::min(name.size(), size);
							return std::memcmp(name.data(), a_elem.Name, len) == 0;
						});
					if (it != sections.end()) {
						section = stl::span{ it->VirtualAddress + _image.data(), it->SizeOfRawData };
					}
				}

				if (!_image.empty() &&
					!_data.empty() &&
					!_rdata.empty()) {
					detail::VTable v{ ".?AVtype_info@@"sv, _image, _data, _rdata };
					_typeInfo = static_cast<const RE::msvc::type_info*>(v.get());
				}
			}

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
			stl::span<const std::byte> _data;
			stl::span<const std::byte> _rdata;
			const RE::msvc::type_info* _typeInfo{ nullptr };
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
						FMT_STRING(" -> {}+0x{:X}"),
						it->id,
						offset - it->offset);
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

	using module_pointer = std::unique_ptr<Modules::Module>;

	namespace Stack
	{
		class Integer
		{
		public:
			[[nodiscard]] std::string name() const noexcept { return "size_t"; }
		};

		class Pointer
		{
		public:
			[[nodiscard]] std::string name() const noexcept { return "void*"; }
		};

		class Polymorphic
		{
		public:
			Polymorphic(
				std::string_view a_mangled,
				const Modules::Module* a_module,
				const RE::RTTI::CompleteObjectLocator* a_col,
				const void* a_ptr) noexcept :
				_mangled{ a_mangled },
				_module{ a_module },
				_col{ a_col },
				_ptr{ a_ptr }
			{
				assert(_mangled.size() > 1 && _mangled.data()[_mangled.size()] == '\0');
				assert(_module != nullptr);
				assert(_col != nullptr);
				assert(_ptr != nullptr);
			}

			[[nodiscard]] std::string name() const noexcept
			{
				std::array<char, 0x1000> buf;
				const auto len =
					::UnDecorateSymbolName(
						_mangled.data() + 1,
						buf.data(),
						static_cast<::DWORD>(buf.size()),
						UNDNAME_NO_MS_KEYWORDS |
							UNDNAME_NO_FUNCTION_RETURNS |
							UNDNAME_NO_ALLOCATION_MODEL |
							UNDNAME_NO_ALLOCATION_LANGUAGE |
							UNDNAME_NO_THISTYPE |
							UNDNAME_NO_ACCESS_SPECIFIERS |
							UNDNAME_NO_THROW_SIGNATURES |
							UNDNAME_NO_RETURN_UDT_MODEL |
							UNDNAME_NAME_ONLY |
							UNDNAME_NO_ARGUMENTS |
							static_cast<::DWORD>(0x8000));	// Disable enum/class/struct/union prefix

				if (len != 0) {
					std::string result(buf.data(), len + 1);
					result.back() = '*';
					return result + analyze();
				} else {
					return "ERROR"s;
				}
			}

		private:
			[[nodiscard]] static std::string filter_TESForm(const void* a_ptr) noexcept
			{
				const auto form = static_cast<const RE::TESForm*>(a_ptr);
				const auto file = form->GetDescriptionOwnerFile();
				return fmt::format(
					FMT_STRING("FormID=0x{:08X} Flags=0x{:08X} File=\"{}\""),
					form->GetFormID(),
					form->GetFormFlags(),
					(file ? file->GetFilename() : ""sv));
			}

			[[nodiscard]] std::string analyze() const noexcept
			{
				const auto hierarchy =
					reinterpret_cast<RE::RTTI::ClassHierarchyDescriptor*>(
						_col->classDescriptor.offset() + _module->address());
				const stl::span bases(
					reinterpret_cast<std::uint32_t*>(
						hierarchy->baseClassArray.offset() + _module->address()),
					hierarchy->numBaseClasses);
				for (const auto rva : bases) {
					const auto base =
						reinterpret_cast<RE::RTTI::BaseClassDescriptor*>(
							rva + _module->address());
					const auto desc =
						reinterpret_cast<RE::RTTI::TypeDescriptor*>(
							base->typeDescriptor.offset() + _module->address());
					const auto it = FILTERS.find(desc->mangled_name());
					if (it != FILTERS.end()) {	// TODO
						const auto root = stl::adjust_pointer<void>(_ptr, -static_cast<std::ptrdiff_t>(_col->offset));
						const auto target = stl::adjust_pointer<void>(root, static_cast<std::ptrdiff_t>(base->pmd.mDisp));
						return " "s + it->second(target);
					}
				}

				return ""s;
			}

			static constexpr auto FILTERS = frozen::make_map({
				std::make_pair(".?AVTESForm@@"sv, filter_TESForm),
			});

			std::string_view _mangled;
			const Modules::Module* _module{ nullptr };
			const RE::RTTI::CompleteObjectLocator* _col{ nullptr };
			const void* _ptr{ nullptr };
		};

		using analysis_result =
			std::variant<
				Integer,
				Pointer,
				Polymorphic>;

		[[nodiscard]] const Modules::Module* get_module_for_pointer(
			void* a_ptr,
			stl::span<const module_pointer> a_modules) noexcept
		{
			const auto it = std::lower_bound(
				a_modules.rbegin(),
				a_modules.rend(),
				reinterpret_cast<std::uintptr_t>(a_ptr),
				[](auto&& a_lhs, auto&& a_rhs) noexcept {
					return a_lhs->address() >= a_rhs;
				});
			return it != a_modules.rend() && (*it)->in_range(a_ptr) ? it->get() : nullptr;
		}

		[[nodiscard]] auto analyze_pointer(
			void* a_ptr,
			stl::span<const module_pointer> a_modules) noexcept
			-> analysis_result
		{
			__try {
				const auto vtable = *reinterpret_cast<void**>(a_ptr);
				const auto mod = get_module_for_pointer(vtable, a_modules);
				if (!mod || !mod->in_rdata_range(vtable)) {
					return Pointer{};
				}

				const auto col =
					*reinterpret_cast<RE::RTTI::CompleteObjectLocator**>(
						reinterpret_cast<std::size_t*>(vtable) - 1);
				if (mod != get_module_for_pointer(col, a_modules) || !mod->in_rdata_range(col)) {
					return Pointer{};
				}

				const auto typeDesc =
					reinterpret_cast<RE::RTTI::TypeDescriptor*>(
						mod->address() + col->typeDescriptor.offset());
				if (mod != get_module_for_pointer(typeDesc, a_modules) || !mod->in_data_range(typeDesc)) {
					return Pointer{};
				}

				if (*reinterpret_cast<const void**>(typeDesc) != mod->type_info()) {
					return Pointer{};
				}

				return Polymorphic{ typeDesc->mangled_name(), mod, col, a_ptr };
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return Pointer{};
			}
		}

		[[nodiscard]] auto analyze_integer(
			std::size_t a_value,
			stl::span<const module_pointer> a_modules) noexcept
			-> analysis_result
		{
			__try {
				*reinterpret_cast<const volatile std::byte*>(a_value);
				return analyze_pointer(reinterpret_cast<void*>(a_value), a_modules);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return Integer{};
			}
		}

		[[nodiscard]] std::vector<std::string> analyze_stack(
			stl::span<const std::size_t> a_stack,
			stl::span<const module_pointer> a_modules) noexcept
		{
			std::vector<std::string> results;
			results.reserve(a_stack.size());
			for (const auto val : a_stack) {
				const auto result = analyze_integer(val, a_modules);
				results.push_back(
					std::visit(
						[](auto&& a_val) noexcept { return a_val.name(); },
						result));
			}
			return results;
		}
	}

	class Callstack
	{
	public:
		Callstack(const ::EXCEPTION_RECORD& a_except) noexcept
		{
			const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(a_except.ExceptionAddress);
			auto it = std::find_if(
				_stacktrace.cbegin(),
				_stacktrace.cend(),
				[&](auto&& a_elem) noexcept {
					return reinterpret_cast<std::uintptr_t>(a_elem.address()) == exceptionAddress;
				});

			if (it == _stacktrace.cend()) {
				it = _stacktrace.cbegin();
			}

			_frames = stl::make_span(it, _stacktrace.cend());
		}

		void print(
			std::shared_ptr<spdlog::logger> a_log,
			stl::span<const module_pointer> a_modules) const noexcept
		{
			assert(a_log != nullptr);
			print_probable_callstack(a_log, a_modules);
			a_log->critical(""sv);
			print_raw_callstack(a_log);
		}

	private:
		[[nodiscard]] static std::string get_size_string(std::size_t a_size) noexcept
		{
			return fmt::to_string(
				fmt::to_string(a_size - 1)
					.length());
		}

		[[nodiscard]] std::string get_format(std::size_t a_nameWidth) const noexcept
		{
			return "\t[{:>"s +
				   get_size_string(_frames.size()) +
				   "}] 0x{:012X} {:>"s +
				   fmt::to_string(a_nameWidth) +
				   "}{}"s;
		}

		void print_probable_callstack(
			std::shared_ptr<spdlog::logger> a_log,
			stl::span<const module_pointer> a_modules) const noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("PROBABLE CALL STACK:"sv);

			std::vector<const Modules::Module*> moduleStack;
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

			const auto format = get_format([&]() noexcept {
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
				const auto& frame = _frames[i];
				a_log->critical(
					format,
					i,
					reinterpret_cast<std::uintptr_t>(frame.address()),
					(mod ? mod->name() : ""sv),
					(mod ? mod->frame_info(frame) : ""s));
			}
		}

		void print_raw_callstack(std::shared_ptr<spdlog::logger> a_log) const noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("RAW CALL STACK:");

			const auto format =
				"\t[{:>"s +
				get_size_string(_stacktrace.size()) +
				"}] 0x{:X}"s;

			for (std::size_t i = 0; i < _stacktrace.size(); ++i) {
				a_log->critical(
					format,
					i,
					reinterpret_cast<std::uintptr_t>(_stacktrace[i].address()));
			}
		}

		boost::stacktrace::stacktrace _stacktrace;
		stl::span<const boost::stacktrace::frame> _frames;
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

#define EXCEPTION_CASE(a_code)                                               \
	case a_code:                                                             \
		a_log->critical(                                                     \
			FMT_STRING("Unhandled exception \"{}\" at 0x{:X}"),              \
			#a_code##sv,                                                     \
			reinterpret_cast<std::uintptr_t>(a_exception.ExceptionAddress)); \
		break

	void print_exception(
		std::shared_ptr<spdlog::logger> a_log,
		const ::EXCEPTION_RECORD& a_exception) noexcept
	{
		assert(a_log != nullptr);

		switch (a_exception.ExceptionCode) {
			EXCEPTION_CASE(EXCEPTION_ACCESS_VIOLATION);
			EXCEPTION_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
			EXCEPTION_CASE(EXCEPTION_BREAKPOINT);
			EXCEPTION_CASE(EXCEPTION_DATATYPE_MISALIGNMENT);
			EXCEPTION_CASE(EXCEPTION_FLT_DENORMAL_OPERAND);
			EXCEPTION_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO);
			EXCEPTION_CASE(EXCEPTION_FLT_INEXACT_RESULT);
			EXCEPTION_CASE(EXCEPTION_FLT_INVALID_OPERATION);
			EXCEPTION_CASE(EXCEPTION_FLT_OVERFLOW);
			EXCEPTION_CASE(EXCEPTION_FLT_STACK_CHECK);
			EXCEPTION_CASE(EXCEPTION_FLT_UNDERFLOW);
			EXCEPTION_CASE(EXCEPTION_ILLEGAL_INSTRUCTION);
			EXCEPTION_CASE(EXCEPTION_IN_PAGE_ERROR);
			EXCEPTION_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO);
			EXCEPTION_CASE(EXCEPTION_INT_OVERFLOW);
			EXCEPTION_CASE(EXCEPTION_INVALID_DISPOSITION);
			EXCEPTION_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION);
			EXCEPTION_CASE(EXCEPTION_PRIV_INSTRUCTION);
			EXCEPTION_CASE(EXCEPTION_SINGLE_STEP);
			EXCEPTION_CASE(EXCEPTION_STACK_OVERFLOW);
		default:
			a_log->critical(
				FMT_STRING("Unhandled exception at 0x{:X}"),
				reinterpret_cast<std::uintptr_t>(a_exception.ExceptionAddress));
			break;
		}
	}

#undef EXCEPTION_CASE

	void print_modules(
		std::shared_ptr<spdlog::logger> a_log,
		stl::span<const module_pointer> a_modules) noexcept
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

	void print_stack(
		std::shared_ptr<spdlog::logger> a_log,
		const ::CONTEXT& a_context,
		stl::span<const module_pointer> a_modules) noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("STACK:"sv);

		const auto tib = reinterpret_cast<const ::NT_TIB*>(::NtCurrentTeb());
		const auto base = tib ? static_cast<const std::size_t*>(tib->StackBase) : nullptr;
		if (!base) {
			a_log->critical("\tFAILED TO READ TIB"sv);
		} else {
			const auto rsp = reinterpret_cast<const std::size_t*>(a_context.Rsp);
			stl::span stack{ rsp, base };

			const auto format = [&]() noexcept {
				return "\t[RSP+{:<"s +
					   fmt::to_string(
						   fmt::format(FMT_STRING("{:X}"), (stack.size() - 1) * sizeof(std::size_t))
							   .length()) +
					   "X}] 0x{:<12X} ({})"s;
			}();

			const auto analysis = Stack::analyze_stack(stack, a_modules);
			for (std::size_t i = 0; i < stack.size(); ++i) {
				a_log->critical(
					format,
					i * sizeof(std::size_t),
					stack[i],
					analysis[i]);
			}
		}
	}

	std::int32_t __stdcall UnhandledExceptions(::EXCEPTION_POINTERS* a_exception) noexcept
	{
#ifndef NDEBUG
		for (; !::IsDebuggerPresent();) {}
#endif

		const auto modules = Modules::get_loaded_modules();
		const auto log = get_log();

		print_exception(log, *a_exception->ExceptionRecord);

		log->critical(""sv);
		const Callstack callstack{ *a_exception->ExceptionRecord };
		callstack.print(log, stl::make_span(modules.begin(), modules.end()));

		log->critical(""sv);
		print_registers(log, *a_exception->ContextRecord);

		log->critical(""sv);
		print_stack(log, *a_exception->ContextRecord, stl::make_span(modules.begin(), modules.end()));

		log->critical(""sv);
		print_modules(log, stl::make_span(modules.begin(), modules.end()));

		log->critical(""sv);
		print_plugins(log);

		log->flush();

		WinAPI::TerminateProcess(
			WinAPI::GetCurrentProcess(),
			EXIT_FAILURE);

		return EXCEPTION_CONTINUE_SEARCH;
	}

	std::int32_t _stdcall VectoredExceptions(::EXCEPTION_POINTERS*) noexcept
	{
		::SetUnhandledExceptionFilter(
			reinterpret_cast<::LPTOP_LEVEL_EXCEPTION_FILTER>(&UnhandledExceptions));
		return EXCEPTION_CONTINUE_SEARCH;
	}

	void Install()
	{
		const auto success =
			::AddVectoredExceptionHandler(
				1,
				reinterpret_cast<::PVECTORED_EXCEPTION_HANDLER>(&VectoredExceptions));
		if (success == nullptr) {
			stl::report_and_fail("failed to install vectored exception handler"sv);
		}
		logger::info("installed crash handlers"sv);
	}
}
