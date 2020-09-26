#include "Patches/SmallBlockAllocatorPatch.h"

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

namespace Patches
{
	struct AllocPatch :
		Xbyak::CodeGenerator
	{
		AllocPatch(std::size_t a_size, std::uintptr_t a_target)
		{
			mov(rcx, a_size);
			mov(rdx, a_target);
			jmp(rdx);
		}
	};

	struct DeallocPatch :
		Xbyak::CodeGenerator
	{
		DeallocPatch(std::uintptr_t a_target)
		{
			mov(rcx, rdx);
			mov(rdx, a_target);
			jmp(rdx);
		}
	};

	void SmallBlockAllocatorPatch::InstallAllocations()
	{
		constexpr std::size_t funcSize = 0xAC;
		constexpr std::array todo{
			std::make_pair<std::size_t, std::uint64_t>(24, 764468),
			std::make_pair<std::size_t, std::uint64_t>(32, 244379),
			std::make_pair<std::size_t, std::uint64_t>(40, 323762),
			std::make_pair<std::size_t, std::uint64_t>(48, 403216),
			std::make_pair<std::size_t, std::uint64_t>(56, 482791),
			std::make_pair<std::size_t, std::uint64_t>(64, 562495),
			std::make_pair<std::size_t, std::uint64_t>(80, 933298),
			std::make_pair<std::size_t, std::uint64_t>(88, 1012687),
		};

		for (const auto& [size, id] : todo) {
			AllocPatch p{ size, reinterpret_cast<std::uintptr_t>(&Allocate) };
			p.ready();
			REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
			assert(p.getSize() <= funcSize);
			REL::safe_write(
				target.address(),
				stl::span{ p.getCode<const std::byte*>(), p.getSize() });
			REL::safe_fill(
				target.address() + p.getSize(),
				REL::INT3,
				funcSize - p.getSize());
		}
	}

	void SmallBlockAllocatorPatch::InstallDeallocations()
	{
		constexpr std::size_t funcSize = 0xB4;
		constexpr std::array todo{
			static_cast<std::uint64_t>(843767),
			static_cast<std::uint64_t>(1357032),
			static_cast<std::uint64_t>(293097),
			static_cast<std::uint64_t>(811714),
			static_cast<std::uint64_t>(1329635),
			static_cast<std::uint64_t>(1037201),
			static_cast<std::uint64_t>(1555743),
			static_cast<std::uint64_t>(491434),
		};

		DeallocPatch p{ reinterpret_cast<std::uintptr_t>(&Deallocate) };
		p.ready();
		assert(p.getSize() <= funcSize);
		for (const auto id : todo) {
			REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
			REL::safe_write(
				target.address(),
				stl::span{ p.getCode<const std::byte*>(), p.getSize() });
			REL::safe_fill(
				target.address() + p.getSize(),
				REL::INT3,
				funcSize - p.getSize());
		}
	}
}
