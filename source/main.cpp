#include <main.hpp>
#include <statusx.hpp>
#include <GarrysMod/Lua/Interface.h>

#if defined __APPLE__

#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED > 1050

#error The only supported compilation platform for this project on Mac OS X is GCC with Mac OS X 10.5 SDK (for ABI reasons).

#endif

#endif

namespace global
{
	SourceSDK::FactoryLoader engine_loader("engine", false, true, "bin/");
	std::string engine_lib = Helpers::GetBinaryFileName("engine", false, true, "bin/");

	static void PreInitialize(GarrysMod::Lua::ILuaBase *LUA)
	{
		if (!engine_loader.IsValid())
			LUA->ThrowError("unable to get engine factory" );

		LUA->CreateTable();

		LUA->PushString("StatusX 1.0");
		LUA->SetField(-2, "Version");
	}

	static void Initialize(GarrysMod::Lua::ILuaBase *LUA)
	{
		LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "statusx");
	}

	static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA)
	{
		LUA->PushNil();
		LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "statusx");
	}

}

GMOD_MODULE_OPEN()
{
	global::PreInitialize(LUA);
	statusx::Initialize(LUA);
	global::Initialize(LUA);
	return 1;
}

GMOD_MODULE_CLOSE()
{
	statusx::Deinitialize(LUA);
	global::Deinitialize(LUA);
	return 0;
}
