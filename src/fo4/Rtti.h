#pragma once
// B.H.O.P. - Finding game classes safely at runtime (RTTI), for hooks that must survive other plugins.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	// =====================================================================
	// memory + RTTI helpers (used to find things without hard-coded addresses)
	// =====================================================================
	bool Readable(const void* a_ptr, std::size_t a_size)
	{
		if (!a_ptr) {
			return false;
		}
		REX::W32::MEMORY_BASIC_INFORMATION mbi{};
		if (!REX::W32::VirtualQuery(a_ptr, &mbi, sizeof(mbi))) {
			return false;
		}
		if (mbi.state != REX::W32::MEM_COMMIT || (mbi.protect & 0x101u) != 0 || mbi.protect == 0) {  // NOACCESS | GUARD
			return false;
		}
		const auto end = reinterpret_cast<std::uintptr_t>(mbi.baseAddress) + mbi.regionSize;
		return reinterpret_cast<std::uintptr_t>(a_ptr) + a_size <= end;
	}

	struct Section
	{
		std::uintptr_t start;
		std::size_t    size;
		bool           exec;
	};

	struct GameModule
	{
		std::uintptr_t       base{ 0 };
		std::uintptr_t       end{ 0 };
		std::vector<Section> data;  // non-executable sections (.rdata, .data)

		bool Contains(std::uintptr_t a_addr, std::size_t a_size = 1) const { return a_addr >= base && a_addr + a_size <= end; }
	};

	const GameModule& Game()
	{
		static GameModule mod = []() {
			GameModule m;
			m.base = reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr));
			if (!m.base) {
				return m;
			}
			const auto nt = m.base + *reinterpret_cast<const std::uint32_t*>(m.base + 0x3C);
			const auto numSections = *reinterpret_cast<const std::uint16_t*>(nt + 6);
			const auto optSize = *reinterpret_cast<const std::uint16_t*>(nt + 20);
			const auto imageSize = *reinterpret_cast<const std::uint32_t*>(nt + 24 + 56);
			m.end = m.base + imageSize;
			auto sec = nt + 24 + optSize;
			for (std::uint16_t i = 0; i < numSections; ++i, sec += 40) {
				const auto vsize = *reinterpret_cast<const std::uint32_t*>(sec + 8);
				const auto rva = *reinterpret_cast<const std::uint32_t*>(sec + 12);
				const auto chars = *reinterpret_cast<const std::uint32_t*>(sec + 36);
				const bool exec = (chars & 0x20000000u) != 0;
				if (!exec) {
					m.data.push_back({ m.base + rva, vsize, exec });
				}
			}
			return m;
		}();
		return mod;
	}

	// ".?AVBGSSoundOutput@@" etc, or nullptr
	const char* RttiName(const void* a_obj)
	{
		const auto& g = Game();
		if (!g.base || !Readable(a_obj, 8)) {
			return nullptr;
		}
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		if (!g.Contains(vtbl - 8, 8)) {
			return nullptr;
		}
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		if (!g.Contains(col, 24) || *reinterpret_cast<const std::uint32_t*>(col) != 1) {
			return nullptr;
		}
		const auto td = g.base + *reinterpret_cast<const std::uint32_t*>(col + 12);
		if (!g.Contains(td + 16, 4)) {
			return nullptr;
		}
		return reinterpret_cast<const char*>(td + 16);
	}

	// Offset of the part of an object that this pointer points at (from the RTTI), or -1.
	std::int32_t SubobjectOffset(const void* a_obj)
	{
		const auto& g = Game();
		if (!g.base || !Readable(a_obj, 8)) {
			return -1;
		}
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		if (!g.Contains(vtbl - 8, 8)) {
			return -1;
		}
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		if (!g.Contains(col, 24) || *reinterpret_cast<const std::uint32_t*>(col) != 1) {
			return -1;
		}
		return static_cast<std::int32_t>(reinterpret_cast<const std::uint32_t*>(col)[1]);
	}

	std::uint32_t ClassDescRva(const void* a_obj)
	{
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		return reinterpret_cast<const std::uint32_t*>(col)[4];
	}

	std::int32_t BaseOffset(std::uint32_t a_classDescRva, std::string_view a_baseSubstr);

	// Turns a BGSSoundOutput pointer (any part of it) into its verified BSISoundOutputModel part.
	const RE::BSISoundOutputModel* SoundOutputInterface(const void* a_ptr)
	{
		const auto cur = SubobjectOffset(a_ptr);
		if (cur < 0) {
			return nullptr;
		}
		const auto complete = reinterpret_cast<std::uintptr_t>(a_ptr) - static_cast<std::uintptr_t>(cur);
		const auto ifaceOff = BaseOffset(ClassDescRva(a_ptr), "BSISoundOutputModel");
		if (ifaceOff < 0) {
			return nullptr;
		}
		const auto iface = reinterpret_cast<const void*>(complete + static_cast<std::uintptr_t>(ifaceOff));
		const char* n = RttiName(iface);
		if (SubobjectOffset(iface) != ifaceOff || !n || std::string_view(n).find("BGSSoundOutput") == std::string_view::npos) {
			return nullptr;
		}
		return static_cast<const RE::BSISoundOutputModel*>(iface);
	}

	struct FoundVtable
	{
		std::uintptr_t vtbl;
		std::uint32_t  subobjectOffset;
		std::uint32_t  classDescRva;
	};

	// Finds every vtable of a class by its RTTI name, e.g. "BGSImpactManager".
	std::vector<FoundVtable> FindVtables(std::string_view a_class)
	{
		std::vector<FoundVtable> out;
		const auto&              g = Game();
		if (!g.base) {
			return out;
		}
		const std::string needle = ".?AV" + std::string(a_class) + "@@";

		std::uint32_t tdRva = 0;
		for (auto& s : g.data) {
			const char* p = reinterpret_cast<const char*>(s.start);
			const auto  n = s.size;
			for (std::size_t i = 16; i + needle.size() + 1 < n; ++i) {
				if (p[i] == '.' && std::memcmp(p + i, needle.data(), needle.size()) == 0 && p[i + needle.size()] == '\0') {
					tdRva = static_cast<std::uint32_t>(s.start + i - 16 - g.base);
					break;
				}
			}
			if (tdRva) {
				break;
			}
		}
		if (!tdRva) {
			return out;
		}

		std::vector<std::uintptr_t> cols;
		for (auto& s : g.data) {
			for (std::size_t i = 0; i + 24 <= s.size; i += 4) {
				const auto a = s.start + i;
				const auto* u = reinterpret_cast<const std::uint32_t*>(a);
				if (u[0] == 1 && u[3] == tdRva && u[5] == static_cast<std::uint32_t>(a - g.base)) {
					cols.push_back(a);
				}
			}
		}
		for (auto col : cols) {
			for (auto& s : g.data) {
				for (std::size_t i = 0; i + 16 <= s.size; i += 8) {
					const auto a = s.start + i;
					if (*reinterpret_cast<const std::uintptr_t*>(a) == col) {
						const auto* u = reinterpret_cast<const std::uint32_t*>(col);
						out.push_back({ a + 8, u[1], u[4] });
					}
				}
			}
		}
		return out;
	}

	// Offset of a base class (whose RTTI name contains a_baseSubstr) inside the class described by a_classDescRva.
	std::int32_t BaseOffset(std::uint32_t a_classDescRva, std::string_view a_baseSubstr)
	{
		const auto& g = Game();
		const auto  chd = g.base + a_classDescRva;
		if (!g.Contains(chd, 16)) {
			return -1;
		}
		const auto num = reinterpret_cast<const std::uint32_t*>(chd)[2];
		const auto arr = g.base + reinterpret_cast<const std::uint32_t*>(chd)[3];
		for (std::uint32_t i = 0; i < num && i < 256; ++i) {
			if (!g.Contains(arr + i * 4, 4)) {
				break;
			}
			const auto bcd = g.base + reinterpret_cast<const std::uint32_t*>(arr)[i];
			if (!g.Contains(bcd, 28)) {
				continue;
			}
			const auto* b = reinterpret_cast<const std::uint32_t*>(bcd);
			const auto  td = g.base + b[0];
			if (!g.Contains(td + 16, 4)) {
				continue;
			}
			const std::string_view name(reinterpret_cast<const char*>(td + 16));
			// a_baseSubstr may hold several parts separated by '|'; all must be in the name
			bool        all = true;
			std::size_t start = 0;
			while (start <= a_baseSubstr.size()) {
				const auto end = a_baseSubstr.find('|', start);
				const auto part = a_baseSubstr.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (!part.empty() && name.find(part) == std::string_view::npos) {
					all = false;
					break;
				}
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1;
			}
			if (all) {
				return static_cast<std::int32_t>(b[2]);  // mdisp
			}
		}
		return -1;
	}

	// Writes every base class of a class to the log (for finding things on new game versions).
	void LogBases(std::uint32_t a_classDescRva)
	{
		const auto& g = Game();
		const auto  chd = g.base + a_classDescRva;
		if (!g.Contains(chd, 16)) {
			return;
		}
		const auto num = reinterpret_cast<const std::uint32_t*>(chd)[2];
		const auto arr = g.base + reinterpret_cast<const std::uint32_t*>(chd)[3];
		for (std::uint32_t i = 0; i < num && i < 64; ++i) {
			if (!g.Contains(arr + i * 4, 4)) {
				break;
			}
			const auto bcd = g.base + reinterpret_cast<const std::uint32_t*>(arr)[i];
			if (!g.Contains(bcd, 28)) {
				continue;
			}
			const auto* b = reinterpret_cast<const std::uint32_t*>(bcd);
			const auto  td = g.base + b[0];
			if (g.Contains(td + 16, 4)) {
				REX::INFO("  base {} at +{:X}", reinterpret_cast<const char*>(td + 16), static_cast<std::int32_t>(b[2]));
			}
		}
	}
}
