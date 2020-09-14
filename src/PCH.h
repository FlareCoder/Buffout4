#pragma once

#pragma warning(push)
#pragma warning(disable : 4200 4324 5053)
#include "F4SE/F4SE.h"
#include "RE/Fallout.h"
#pragma warning(pop)

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

namespace WinAPI
{
	void OutputDebugStringA(const char* a_outputString);
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
