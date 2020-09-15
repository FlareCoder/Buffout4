#pragma once

namespace Crash
{
	namespace Modules
	{
		class Module;
	}

	namespace Stack
	{
		[[nodiscard]] std::vector<std::string> analyze_stack(
			stl::span<const std::size_t> a_stack,
			stl::span<const std::unique_ptr<Modules::Module>> a_modules) noexcept;
	}
}
