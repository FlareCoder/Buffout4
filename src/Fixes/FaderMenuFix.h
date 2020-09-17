#pragma once

namespace Fixes
{
	class FaderMenuFix
	{
	public:
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::ID(518701), 0x74 };
			auto& trampoline = F4SE::GetTrampoline();
			_original = trampoline.write_call<5>(target.address(), Hook);
			logger::info("installed {}"sv, typeid(FaderMenuFix).name());
		}

	private:
		static void Hook(void* a_ptr)
		{
			const auto ui = RE::UI::GetSingleton();
			const auto fader = ui ? ui->GetMenu("FaderMenu"sv) : nullptr;
			if (fader && fader->menuObj.IsObject()) {
				std::array todo{
					"FadeRect_mc"sv,
					"SpinnerIcon_mc"sv,
				};

				auto& root = fader->menuObj;
				RE::Scaleform::GFx::Value clip;
				for (const auto& path : todo) {
					if (root.GetMember(path, std::addressof(clip)) && clip.IsObject()) {
						clip.SetMember("alpha"sv, 0.0);
					}
				}
			}

			_original(a_ptr);
		}

		static inline REL::Relocation<decltype(Hook)> _original;
	};
}
