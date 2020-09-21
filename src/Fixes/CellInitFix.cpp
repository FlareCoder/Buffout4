#include "Fixes/CellInitFix.h"

#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <xbyak/xbyak.h>

namespace Fixes
{
	struct Patch :
		Xbyak::CodeGenerator
	{
		Patch(std::uintptr_t a_target)
		{
			mov(rcx, rbx);	// rbx == TESObjectCELL*
			mov(rdx, a_target);
			jmp(rdx);
		}
	};

	void CellInitFix::Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(868663), 0x3E };

		Patch p{ reinterpret_cast<std::uintptr_t>(&GetLocation) };
		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_call<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed {}"sv, typeid(CellInitFix).name());
	}
}
