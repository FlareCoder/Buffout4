#include "Fixes/Fixes.h"

#include "Fixes/CellInitFix.h"
#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/FaderMenuFix.h"
#include "Fixes/SafeExit.h"
#include "Fixes/ScaleformAllocatorFix.h"
#include "Fixes/SmallBlockAllocatorFix.h"

namespace Fixes
{
	void InstallEarly()
	{
		Settings::load();

		if (*Settings::CellInit) {
			CellInitFix::Install();
		}

		if (*Settings::FaderMenu) {
			FaderMenuFix::Install();
		}

		if (*Settings::SafeExit) {
			SafeExit::Install();
		}

		if (*Settings::ScaleformAllocator) {
			ScaleformAllocatorFix::Install();
		}

		if (*Settings::SmallBlockAllocator) {
			SmallBlockAllocatorFix::Install();
		}
	}

	void InstallLate()
	{
		if (*Settings::EncounterZoneReset) {
			EncounterZoneResetFix::Install();
		}
	}
}
