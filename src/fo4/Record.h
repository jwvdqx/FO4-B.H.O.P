#pragma once
// B.H.O.P. - Recording mode: writes a few seconds of movement, step by step, to BHOP_record.csv
// (next to BHOP.log). tests/replay.cpp reads it back and runs the movement core on the same moments.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	namespace Record
	{
		struct Row
		{
			float t, dt;
			int   rawGround, ground;
			float wishX, wishY, wishSpeed, stickX, stickY, yawRate;
			float askedX, askedY, askedVz;
			float actX, actY, repX, repY;
			float gnx, gny, gnz;
			int   hasGround;
			float simBeforeX, simBeforeY;
			int   contact;
			float simX, simY;  // start of the Half-Life steps (after walls, pushes, jump boost)
			float frictionMult, accelMult, airAccelMult, airSurface, grace;
			int   smoothing;
			float outX, outY;
			float inX, inY, inVz, outVz, measVz;
		};

		std::vector<Row> g_rows;
		bool             g_on{ false };
		float            g_left{ 0.0f };
		float            g_t{ 0.0f };
		std::string      g_header;

		std::filesystem::path FilePath()
		{
			wchar_t*   buf{ nullptr };
			const auto res = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(buf));
			std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> known(buf, REX::W32::CoTaskMemFree);
			if (!known || res != 0) {
				return {};
			}
			std::filesystem::path p = known.get();
			p /= std::format("My Games/{}/F4SE/BHOP_record.csv", F4SE::GetSaveFolderName());
			return p;
		}

		// main thread, settings lock held: start a fresh recording
		void Start(float a_seconds)
		{
			g_rows.clear();
			g_rows.reserve(4096);
			g_on = true;
			g_left = a_seconds;
			g_t = 0.0f;
			g_header = std::format("# BHOP record | mode {} | friction {} accelerate {} stopspeed {} tick {} | airaccelerate {} aircap {} assist {} | unitscale {}",
				static_cast<int>(g_cfg.mode), g_active.friction, g_active.accelerate, g_active.stopSpeed, g_active.tick,
				g_cfg.airAccelerate, g_cfg.airCap, g_cfg.strafeAssist, g_cfg.unitScale);
			REX::INFO("Recording movement for {:.0f} s", a_seconds);
		}

		void Write(std::vector<Row> a_rows, std::string a_header)
		{
			const auto path = FilePath();
			std::ofstream f(path, std::ios::trunc);
			if (!f) {
				REX::WARN("Could not write the movement recording");
				return;
			}
			f << a_header << '\n';
			f << "t,dt,rawGround,ground,wishX,wishY,wishSpeed,stickX,stickY,yawRate,askedX,askedY,askedVz,actX,actY,repX,repY,"
			     "gnx,gny,gnz,hasGround,simBeforeX,simBeforeY,contact,simX,simY,frictionMult,accelMult,airAccelMult,airSurface,grace,"
			     "smoothing,outX,outY,inX,inY,inVz,outVz,measVz\n";
			for (const auto& r : a_rows) {
				f << std::format("{:.4f},{:.5f},{},{},{:.4f},{:.4f},{:.2f},{:.3f},{:.3f},{:.3f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},"
				                 "{:.4f},{:.4f},{:.4f},{},{:.2f},{:.2f},{},{:.2f},{:.2f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},"
				                 "{},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
					r.t, r.dt, r.rawGround, r.ground, r.wishX, r.wishY, r.wishSpeed, r.stickX, r.stickY, r.yawRate, r.askedX, r.askedY, r.askedVz,
					r.actX, r.actY, r.repX, r.repY, r.gnx, r.gny, r.gnz, r.hasGround, r.simBeforeX, r.simBeforeY, r.contact, r.simX, r.simY,
					r.frictionMult, r.accelMult, r.airAccelMult, r.airSurface, r.grace, r.smoothing, r.outX, r.outY, r.inX, r.inY, r.inVz, r.outVz, r.measVz);
			}
			REX::INFO("Movement recording written: {} steps to {}", a_rows.size(), path.string());
		}

		// physics thread, settings lock held
		void Add(const Row& a_row)
		{
			if (!g_on) {
				return;
			}
			g_rows.push_back(a_row);
			g_rows.back().t = g_t;
			g_t += a_row.dt;
			g_left -= a_row.dt;
			if (g_left <= 0.0f || g_rows.size() > 20000) {
				g_on = false;
				if (auto task = F4SE::GetTaskInterface()) {
					auto rows = std::make_shared<std::vector<Row>>(std::move(g_rows));
					auto header = std::make_shared<std::string>(g_header);
					task->AddTask([rows, header]() { Write(std::move(*rows), *header); });  // file writing on the main thread
				}
				g_rows = {};
			}
		}
	}
}
