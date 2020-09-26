#pragma once

namespace Crash
{
	namespace Modules
	{
		class Module;
	}

	namespace Introspection
	{
		[[nodiscard]] std::vector<std::string> analyze_data(
			stl::span<const std::size_t> a_data,
			stl::span<const std::unique_ptr<Modules::Module>> a_modules) noexcept;
	}
}
