#pragma once

namespace Fixes
{
	class ActorIsHostileToActorFix
	{
	public:
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(1022223) };

			REL::safe_fill(target.address(), REL::INT3, 0xB);

			auto& trampoline = F4SE::GetTrampoline();
			trampoline.write_branch<5>(target.address(), IsHostileToActor);
		}

	private:
		static bool IsHostileToActor(RE::BSScript::IVirtualMachine* a_vm, std::uint32_t a_stackID, RE::Actor* a_self, RE::Actor* a_actor)
		{
			if (!a_actor) {
				RE::GameScript::LogFormError(
					a_actor,
					"Cannot call IsHostileToActor with a None actor",
					a_vm,
					a_stackID);
				return false;
			} else {
				return a_self->GetHostileToActor(a_actor);
			}
		}
	};
}
