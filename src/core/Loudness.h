#pragma once
// B.H.O.P. - How loud a sound file is, for footstep loudness equalization.
// Pure C++ (no Fallout code), unit-tested in tests/test_hlmath.cpp.
//
// Footsteps are short and punchy, so what you hear as "how loud" is the loudest moment, not the average
// over the whole file (most of a step file is near silence). Measured: the RMS level of the loudest 50 ms,
// in dB relative to full scale (0 dB = as loud as the format allows, quieter is negative).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace loudness
{
	struct Wav
	{
		int                channels{ 0 };
		int                rate{ 0 };
		std::vector<float> mono;  // samples -1..1, channels mixed down
	};

	inline std::uint32_t U32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
	inline std::uint16_t U16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

	// Reads a RIFF/WAVE file in memory: PCM 8/16/24/32-bit or 32-bit float. Anything else (compressed
	// formats, .xwm): nothing.
	inline std::optional<Wav> ParseWav(const std::uint8_t* a_data, std::size_t a_size)
	{
		if (!a_data || a_size < 12 || std::memcmp(a_data, "RIFF", 4) != 0 || std::memcmp(a_data + 8, "WAVE", 4) != 0) {
			return std::nullopt;
		}
		int                  format = 0, channels = 0, rate = 0, bits = 0;
		const std::uint8_t*  samples = nullptr;
		std::size_t          sampleBytes = 0;
		std::size_t          pos = 12;
		while (pos + 8 <= a_size) {
			const std::uint32_t len = U32(a_data + pos + 4);
			const std::uint8_t* body = a_data + pos + 8;
			const std::size_t   avail = std::min<std::size_t>(len, a_size - pos - 8);
			if (std::memcmp(a_data + pos, "fmt ", 4) == 0 && avail >= 16) {
				format = U16(body);
				channels = U16(body + 2);
				rate = static_cast<int>(U32(body + 4));
				bits = U16(body + 14);
				if (format == 0xFFFE && avail >= 26) {
					format = U16(body + 24);  // WAVE_FORMAT_EXTENSIBLE: the real format is in the sub-format
				}
			} else if (std::memcmp(a_data + pos, "data", 4) == 0) {
				samples = body;
				sampleBytes = avail;
			}
			pos += 8 + len + (len & 1);
		}
		const bool pcm = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
		const bool flt = format == 3 && bits == 32;
		if (!samples || channels < 1 || channels > 8 || rate < 1000 || (!pcm && !flt)) {
			return std::nullopt;
		}
		const std::size_t bytes = static_cast<std::size_t>(bits / 8);
		const std::size_t frame = bytes * static_cast<std::size_t>(channels);
		const std::size_t frames = sampleBytes / frame;
		Wav w;
		w.channels = channels;
		w.rate = rate;
		w.mono.resize(frames);
		for (std::size_t f = 0; f < frames; ++f) {
			float sum = 0.0f;
			for (int c = 0; c < channels; ++c) {
				const std::uint8_t* s = samples + f * frame + static_cast<std::size_t>(c) * bytes;
				float v = 0.0f;
				if (flt) {
					std::memcpy(&v, s, 4);
				} else if (bits == 8) {
					v = (static_cast<int>(s[0]) - 128) / 128.0f;
				} else if (bits == 16) {
					v = static_cast<std::int16_t>(U16(s)) / 32768.0f;
				} else if (bits == 24) {
					std::int32_t x = s[0] | (s[1] << 8) | (s[2] << 16);
					if (x & 0x800000) {
						x |= ~0xFFFFFF;
					}
					v = x / 8388608.0f;
				} else {
					v = static_cast<std::int32_t>(U32(s)) / 2147483648.0f;
				}
				sum += std::isfinite(v) ? v : 0.0f;
			}
			w.mono[f] = sum / static_cast<float>(channels);
		}
		return w;
	}

	// RMS level of the loudest a_windowMs stretch, dB full scale. Silence: -120.
	inline float PeakWindowDb(const Wav& a_wav, float a_windowMs = 50.0f)
	{
		const std::size_t n = a_wav.mono.size();
		if (n == 0 || a_wav.rate <= 0) {
			return -120.0f;
		}
		const std::size_t win = std::clamp<std::size_t>(static_cast<std::size_t>(a_wav.rate * a_windowMs / 1000.0f), 1, n);
		double            sum = 0.0;
		for (std::size_t i = 0; i < win; ++i) {
			sum += static_cast<double>(a_wav.mono[i]) * a_wav.mono[i];
		}
		double best = sum;
		for (std::size_t i = win; i < n; ++i) {
			sum += static_cast<double>(a_wav.mono[i]) * a_wav.mono[i] - static_cast<double>(a_wav.mono[i - win]) * a_wav.mono[i - win];
			best = std::max(best, sum);
		}
		const double rms = std::sqrt(std::max(0.0, best) / static_cast<double>(win));
		return rms > 1e-6 ? static_cast<float>(20.0 * std::log10(rms)) : -120.0f;
	}

	// The level everything is brought to: a low percentile of all measured files, so most files are turned
	// down to match the quieter ones (sounds can only be turned down, never past their own level), while a
	// few unusually quiet files don't drag everything down.
	inline float TargetDb(std::vector<float> a_levels, float a_percentile = 0.2f)
	{
		a_levels.erase(std::remove_if(a_levels.begin(), a_levels.end(), [](float l) { return l <= -100.0f; }), a_levels.end());
		if (a_levels.empty()) {
			return 0.0f;
		}
		std::sort(a_levels.begin(), a_levels.end());
		const auto i = static_cast<std::size_t>(std::clamp(a_percentile, 0.0f, 1.0f) * static_cast<float>(a_levels.size() - 1));
		return a_levels[i];
	}

	// Volume factor (0..1) for a file at a_levelDb to sound like a_targetDb. Louder files are turned down
	// (at most a_maxCutDb); quieter ones stay as they are.
	inline float Gain(float a_levelDb, float a_targetDb, float a_maxCutDb = 18.0f)
	{
		if (a_levelDb <= -100.0f || a_levelDb <= a_targetDb) {
			return 1.0f;
		}
		const float cut = std::min(a_levelDb - a_targetDb, a_maxCutDb);
		return std::pow(10.0f, -cut / 20.0f);
	}
}
