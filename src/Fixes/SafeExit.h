#pragma once

namespace Fixes
{
	class SafeExit
	{
	public:
		static void Install()
		{
			auto& trampoline = F4SE::GetTrampoline();
			REL::Relocation<std::uintptr_t> target{ REL::ID(668528), 0x20 };
			trampoline.write_call<5>(target.address(), Shutdown);
			logger::info("installed {}"sv, typeid(SafeExit).name());
		}

	private:
		static void Shutdown()
		{
			WinAPI::TerminateProcess(WinAPI::GetCurrentProcess(), EXIT_SUCCESS);
		}
	};
}
