#pragma once

namespace Patches
{
	class MemoryManagerPatch
	{
	public:
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(1283094) };
			REL::safe_fill(target.address(), REL::INT3, 0xBF);
			REL::safe_write(target.address(), REL::RET);
			logger::info("installed {}"sv, typeid(MemoryManagerPatch).name());
		}
	};
}
