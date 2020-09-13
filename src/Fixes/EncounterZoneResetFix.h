#pragma once

namespace Fixes
{
	class EncounterZoneResetFix :
		public RE::BSTEventSink<RE::CellAttachDetachEvent>
	{
	public:
		[[nodiscard]] static EncounterZoneResetFix* GetSingleton()
		{
			static EncounterZoneResetFix singleton;
			return std::addressof(singleton);
		}

		static void Install()
		{
			auto& cells = RE::CellAttachDetachEventSource::CellAttachDetachEventSourceSingleton::GetSingleton();
			cells.source.RegisterSink(GetSingleton());
			logger::info(FMT_STRING("installed {}"), typeid(EncounterZoneResetFix).name());
		}

	private:
		EncounterZoneResetFix() = default;
		EncounterZoneResetFix(const EncounterZoneResetFix&) = delete;
		EncounterZoneResetFix(EncounterZoneResetFix&&) = delete;

		~EncounterZoneResetFix() = default;

		EncounterZoneResetFix& operator=(const EncounterZoneResetFix&) = delete;
		EncounterZoneResetFix& operator=(EncounterZoneResetFix&&) = delete;

		RE::BSEventNotifyControl ProcessEvent(const RE::CellAttachDetachEvent& a_event, RE::BSTEventSource<RE::CellAttachDetachEvent>*) override
		{
			switch (*a_event.type) {
			case RE::CellAttachDetachEvent::EVENT_TYPE::kPreDetach:
				{
					const auto cell = a_event.cell;
					const auto ez = cell ? cell->GetEncounterZone() : nullptr;
					const auto calendar = RE::Calendar::GetSingleton();
					if (ez && calendar) {
						ez->SetDetachTime(
							static_cast<std::uint32_t>(calendar->GetHoursPassed()));
					}
				}
				break;
			default:
				break;
			}

			return RE::BSEventNotifyControl::kContinue;
		}
	};
}
