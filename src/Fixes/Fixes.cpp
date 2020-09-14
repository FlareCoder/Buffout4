#include "Fixes/Fixes.h"

#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/SafeExit.h"
#include "Fixes/ScaleformAllocatorFix.h"
#include "Fixes/SmallBlockAllocatorFix.h"

#include "Settings.h"

namespace Fixes
{
	void InstallEarly()
	{
		Settings::load();

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
