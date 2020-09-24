#pragma once

class Settings
{
public:
	using ISetting = AutoTOML::ISetting;
	using bSetting = AutoTOML::bSetting;

	static void load()
	{
		try {
			const auto table = toml::parse_file("Data/F4SE/Plugins/Buffout4.toml"s);
			for (const auto& setting : ISetting::get_settings()) {
				setting->load(table);
			}
		} catch (const toml::parse_error& e) {
			std::ostringstream ss;
			ss
				<< "Error parsing file \'" << *e.source().path << "\':\n"
				<< '\t' << e.description() << '\n'
				<< "\t\t(" << e.source().begin << ')';
			logger::error(ss.str());
			stl::report_and_fail("failed to load settings"sv);
		} catch (const std::exception& e) {
			stl::report_and_fail(e.what());
		} catch (...) {
			stl::report_and_fail("unknown failure"sv);
		}
	}

	static inline bSetting ActorIsHostileToActor{ "Fixes"s, "ActorIsHostileToActor"s, true };
	static inline bSetting CellInit{ "Fixes"s, "CellInit"s, true };
	static inline bSetting EncounterZoneReset{ "Fixes"s, "EncounterZoneReset"s, true };
	static inline bSetting FaderMenu{ "Fixes"s, "FaderMenu"s, true };
	static inline bSetting SafeExit{ "Fixes"s, "SafeExit"s, true };
	static inline bSetting ScaleformAllocator{ "Fixes"s, "ScaleformAllocator"s, true };
	static inline bSetting SmallBlockAllocator{ "Fixes"s, "SmallBlockAllocator"s, true };

private:
	Settings() = delete;
	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;

	~Settings() = delete;

	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;
};
