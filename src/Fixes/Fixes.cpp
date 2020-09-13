#include "Fixes/Fixes.h"

#include "Fixes/ScaleformAllocator.h"

namespace Fixes
{
	void Install()
	{
		ScaleformAllocator::Install();
		logger::info("installed all fixes"sv);
	}
}
