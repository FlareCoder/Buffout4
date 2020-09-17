#pragma once

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <execution>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include <boost/algorithm/searching/knuth_morris_pratt.hpp>
#include <fmt/chrono.h>
#include <frozen/map.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "AutoTOML.hpp"

namespace WinAPI
{
	inline constexpr auto(EXCEPTION_EXECUTE_HANDLER){ static_cast<int>(1) };

	inline constexpr auto(UNDNAME_NO_MS_KEYWORDS){ static_cast<std::uint32_t>(0x0002) };
	inline constexpr auto(UNDNAME_NO_FUNCTION_RETURNS){ static_cast<std::uint32_t>(0x0004) };
	inline constexpr auto(UNDNAME_NO_ALLOCATION_MODEL){ static_cast<std::uint32_t>(0x0008) };
	inline constexpr auto(UNDNAME_NO_ALLOCATION_LANGUAGE){ static_cast<std::uint32_t>(0x0010) };
	inline constexpr auto(UNDNAME_NO_THISTYPE){ static_cast<std::uint32_t>(0x0060) };
	inline constexpr auto(UNDNAME_NO_ACCESS_SPECIFIERS){ static_cast<std::uint32_t>(0x0080) };
	inline constexpr auto(UNDNAME_NO_THROW_SIGNATURES){ static_cast<std::uint32_t>(0x0100) };
	inline constexpr auto(UNDNAME_NO_RETURN_UDT_MODEL){ static_cast<std::uint32_t>(0x0400) };
	inline constexpr auto(UNDNAME_NAME_ONLY){ static_cast<std::uint32_t>(0x1000) };
	inline constexpr auto(UNDNAME_NO_ARGUMENTS){ static_cast<std::uint32_t>(0x2000) };

	void(OutputDebugStringA)(
		const char* a_outputString) noexcept;

	[[nodiscard]] std::uint32_t(UnDecorateSymbolName)(
		const char* a_name,
		char* a_outputString,
		std::uint32_t a_maxStringLength,
		std::uint32_t a_flags) noexcept;
}

#ifndef NDEBUG
#include <spdlog/sinks/base_sink.h>

namespace logger
{
	template <class Mutex>
	class msvc_sink :
		public spdlog::sinks::base_sink<Mutex>
	{
	private:
		using super = spdlog::sinks::base_sink<Mutex>;

	public:
		explicit msvc_sink() {}

	protected:
		void sink_it_(const spdlog::details::log_msg& a_msg) override
		{
			spdlog::memory_buf_t formatted;
			super::formatter_->format(a_msg, formatted);
			WinAPI::OutputDebugStringA(fmt::to_string(formatted).c_str());
		}

		void flush_() override {}
	};

	using msvc_sink_mt = msvc_sink<std::mutex>;
	using msvc_sink_st = msvc_sink<spdlog::details::null_mutex>;

	using windebug_sink_mt = msvc_sink_mt;
	using windebug_sink_st = msvc_sink_st;
}
#endif

#define DLLEXPORT __declspec(dllexport)

namespace logger
{
	using namespace F4SE::log;
}

namespace stl
{
	using F4SE::stl::report_and_fail;
	using F4SE::stl::span;
	using F4SE::util::adjust_pointer;

	template <class, class = void>
	struct iter_reference;

	template <class T>
	struct iter_reference<
		T,
		std::void_t<
			decltype(*std::declval<T&>())>>
	{
		using type = decltype(*std::declval<T&>());
	};

	template <class T>
	using iter_reference_t = typename iter_reference<T>::type;

	template <
		class It,
		class End,
		std::enable_if_t<
			std::conjunction_v<
				std::is_base_of<
					std::random_access_iterator_tag,
					typename std::iterator_traits<It>::iterator_category>,
				std::negation<
					std::is_convertible<End, std::size_t>>>,
			int> = 0>
	[[nodiscard]] auto make_span(It a_first, End a_last)
		-> span<
			std::remove_reference_t<
				iter_reference_t<It>>>
	{
		if (a_first != a_last) {
			return { std::addressof(*a_first), static_cast<std::size_t>(a_last - a_first) };
		} else {
			return {};
		}
	}
}

namespace WinAPI
{
	using namespace F4SE::WinAPI;
}

using namespace std::literals;

#include "Settings.h"
