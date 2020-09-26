set(headers ${headers}
	include/AutoTOML.hpp
	src/PCH.h
	src/Settings.h
	src/Crash/CrashHandler.h
	src/Crash/Introspection/Introspection.h
	src/Crash/Modules/ModuleHandler.h
	src/Fixes/ActorIsHostileToActorFix.h
	src/Fixes/CellInitFix.h
	src/Fixes/EncounterZoneResetFix.h
	src/Fixes/FaderMenuFix.h
	src/Fixes/Fixes.h
	src/Fixes/SafeExit.h
	src/Patches/MemoryManagerPatch.h
	src/Patches/Patches.h
	src/Patches/ScaleformAllocatorPatch.h
	src/Patches/SmallBlockAllocatorPatch.h
)
