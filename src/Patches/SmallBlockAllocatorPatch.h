#pragma once

namespace Patches
{
	class SmallBlockAllocatorPatch
	{
	public:
		static void Install()
		{
			InstallAllocations();
			InstallDeallocations();

			REL::Relocation<std::uintptr_t> target{ REL::ID(329149), 0x48 };
			REL::safe_fill(target.address(), REL::NOP, 0x5);

			logger::info("installed {}"sv, typeid(SmallBlockAllocatorPatch).name());
		}

	private:
		static void InstallAllocations();
		static void InstallDeallocations();

		static void* Allocate(std::size_t a_size) { return std::malloc(a_size); }
		static void Deallocate(void* a_ptr) { std::free(a_ptr); }
	};
}
