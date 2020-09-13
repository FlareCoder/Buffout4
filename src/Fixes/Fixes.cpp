#include "Fixes/Fixes.h"

#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/ScaleformAllocatorFix.h"

#include "Settings.h"

namespace Fixes
{
	void InstallEarly()
	{
		Settings::load();

		if (*Settings::ScaleformAllocator) {
			ScaleformAllocatorFix::Install();
		}
	}

	void InstallLate()
	{
		if (*Settings::EncounterZoneReset) {
			EncounterZoneResetFix::Install();
		}
	}
}
