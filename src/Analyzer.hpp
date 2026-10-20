#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <iomanip>
#include <map>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace dynamic {

constexpr double kPi = 3.14159265358979323846;

using Clock = std::chrono::steady_clock;

inline double msBetween(Clock::time_point a, Clock::time_point b) {
	return std::chrono::duration<double, std::milli>(b - a).count();
}

struct ClickRecord {
	int step = 0;          // physics step the game handled the click on
	bool down = false;     // press (true) or release (false)
	int button = 0;
	bool matched = false;  // was there a real hardware input shortly before?
	double gapMs = 0.0;    // frame start - hardware input time
	double phase = 0.0;    // gap folded into one frame period, 0..1
};

struct Summary {
	size_t total = 0;
	size_t presses = 0;
	size_t matched = 0;
	size_t unmatched = 0;
	double unmatchedRatio = 0.0;

	size_t phaseCount = 0;
	double phaseR = 0.0;     // mean resultant length, 0 = uniform, 1 = locked
	double phaseP = 1.0;     // Rayleigh p-value (small = non-uniform)
	double gapSd = 0.0;      // std dev of gap in ms

	size_t intervalCount = 0;
	double intervalMean = 0.0;
	double intervalSd = 0.0;
	int intervalMin = 0;
	double repeatedFraction = 0.0;

	std::vector<std::string> flags;
};

class Analyzer {
public:
	static Analyzer& get() {
		static Analyzer instance;
		return instance;
	}

	void reset() {
		m_records.clear();
		m_pendingInputs.clear();
	}

	// Called from the keyboard / touch dispatchers: a real hardware input arrived.
	void rawInput() {
		m_pendingInputs.push_back(Clock::now());
		if (m_pendingInputs.size() > 64) m_pendingInputs.pop_front();
	}

	// Called at the start of every game-layer update.
	void beginFrame(double windowMs) {
		auto now = Clock::now();
		if (m_haveFrame) {
			double wall = msBetween(m_frameStart, now);
			if (wall > 0.5 && wall < 100.0) {
				m_period = m_period <= 0.0 ? wall : m_period * 0.9 + wall * 0.1;
			}
		}
		m_frameStart = now;
		m_haveFrame = true;
		while (!m_pendingInputs.empty() && msBetween(m_pendingInputs.front(), now) > windowMs) {
			m_pendingInputs.pop_front();
		}
	}

	// Called from GJBaseGameLayer::handleButton.
	void onButton(bool down, int button, int step) {
		ClickRecord rec;
		rec.step = step;
		rec.down = down;
		rec.button = button;
		if (!m_pendingInputs.empty()) {
			rec.matched = true;
			auto t = m_pendingInputs.front();
			m_pendingInputs.pop_front();
			rec.gapMs = std::max(0.0, msBetween(t, m_frameStart));
			double period = m_period > 0.5 ? m_period : 16.667;
			rec.phase = std::fmod(rec.gapMs, period) / period;
		}
		m_records.push_back(rec);
	}

	size_t clickCount() const { return m_records.size(); }

	Summary summarize(size_t minClicks) const {
		Summary s;
		s.total = m_records.size();

		std::vector<double> gaps;
		std::vector<double> phases;
		std::vector<int> pressSteps;
		for (auto const& r : m_records) {
			if (r.matched) {
				++s.matched;
				gaps.push_back(r.gapMs);
				phases.push_back(r.phase);
			} else {
				++s.unmatched;
			}
			if (r.down) {
				++s.presses;
				pressSteps.push_back(r.step);
			}
		}
		s.unmatchedRatio = s.total ? double(s.unmatched) / double(s.total) : 0.0;

		// Phase concentration (Rayleigh test)
		s.phaseCount = phases.size();
		if (!phases.empty()) {
			double c = 0.0, sn = 0.0;
			for (double p : phases) {
				c += std::cos(2.0 * kPi * p);
				sn += std::sin(2.0 * kPi * p);
			}
			double n = double(phases.size());
			double rSum = std::sqrt(c * c + sn * sn);
			s.phaseR = rSum / n;
			double expo = std::sqrt(1.0 + 4.0 * n + 4.0 * (n * n - rSum * rSum)) - (1.0 + 2.0 * n);
			s.phaseP = std::min(1.0, std::exp(expo));
		}
		s.gapSd = stddev(gaps);

		// Press-to-press intervals in physics steps
		std::vector<double> intervals;
		std::map<int, size_t> freq;
		for (size_t i = 1; i < pressSteps.size(); ++i) {
			int d = pressSteps[i] - pressSteps[i - 1];
			if (d <= 0) continue;
			intervals.push_back(double(d));
			++freq[d];
		}
		s.intervalCount = intervals.size();
		if (!intervals.empty()) {
			double sum = 0.0;
			for (double v : intervals) sum += v;
			s.intervalMean = sum / double(intervals.size());
			s.intervalSd = stddev(intervals);
			s.intervalMin = int(*std::min_element(intervals.begin(), intervals.end()));
			size_t best = 0;
			for (auto const& [k, v] : freq) best = std::max(best, v);
			s.repeatedFraction = double(best) / double(intervals.size());
		}

		// Flags (only once there is enough data)
		if (s.total >= minClicks) {
			if (s.unmatchedRatio >= 0.20) {
				s.flags.push_back(fmt("%.0f%% of in-game inputs had no matching keyboard/mouse/touch event", s.unmatchedRatio * 100.0));
			}
			if (s.phaseCount >= minClicks && s.phaseP < 0.001) {
				s.flags.push_back(fmt("Inputs are phase-locked to the frame (R=%.2f, p=%.1e); real input is normally uniform", s.phaseR, s.phaseP));
			}
			if (s.phaseCount >= minClicks && s.gapSd < 0.25) {
				s.flags.push_back(fmt("Input-to-frame delay is unnaturally constant (sd %.3f ms)", s.gapSd));
			}
			if (s.intervalCount >= minClicks && s.repeatedFraction >= 0.70) {
				s.flags.push_back(fmt("%.0f%% of press intervals are identical (informational, spam sections do this too)", s.repeatedFraction * 100.0));
			}
		}
		return s;
	}

	std::string report(size_t minClicks, std::string const& levelName) const {
		Summary s = summarize(minClicks);
		std::ostringstream o;
		o << "Dynamic input analysis\n";
		o << "Level: " << levelName << "\n\n";
		o << "Inputs handled: " << s.total << " (" << s.presses << " presses)\n";
		o << "Matched to real input: " << s.matched << "\n";
		o << "Unmatched: " << s.unmatched << fmt(" (%.1f%%)", s.unmatchedRatio * 100.0) << "\n";
		o << fmt("Phase concentration R: %.3f (p=%.2e, n=%zu)\n", s.phaseR, s.phaseP, s.phaseCount);
		o << fmt("Input delay spread: %.3f ms\n", s.gapSd);
		o << fmt("Press interval: mean %.1f, sd %.1f, min %d steps, %.0f%% repeated\n",
			s.intervalMean, s.intervalSd, s.intervalMin, s.repeatedFraction * 100.0);
		o << "\n";
		if (s.total < minClicks) {
			o << "Verdict: not enough clicks to judge (" << s.total << "/" << minClicks << ")\n";
		} else if (s.flags.empty()) {
			o << "Verdict: no anomalies found\n";
		} else {
			o << "Verdict: SUSPICIOUS (" << s.flags.size() << " flag" << (s.flags.size() == 1 ? "" : "s") << ")\n";
			for (auto const& f : s.flags) o << " - " << f << "\n";
		}
		o << "\nThis is a statistical heuristic, not proof. Review footage and context before accusing anyone.\n";
		return o.str();
	}

	std::string hudText(size_t minClicks) const {
		Summary s = summarize(minClicks);
		return fmt("Dynamic  clicks %zu  unmatched %.0f%%  R %.2f%s",
			s.total, s.unmatchedRatio * 100.0, s.phaseR, s.flags.empty() ? "" : "  !");
	}

private:
	static double stddev(std::vector<double> const& v) {
		if (v.size() < 2) return 0.0;
		double mean = 0.0;
		for (double x : v) mean += x;
		mean /= double(v.size());
		double acc = 0.0;
		for (double x : v) acc += (x - mean) * (x - mean);
		return std::sqrt(acc / double(v.size() - 1));
	}

	template <typename... Args>
	static std::string fmt(char const* f, Args... args) {
		char buf[256];
		std::snprintf(buf, sizeof(buf), f, args...);
		return buf;
	}

	std::vector<ClickRecord> m_records;
	std::deque<Clock::time_point> m_pendingInputs;
	Clock::time_point m_frameStart{};
	bool m_haveFrame = false;
	double m_period = 0.0;
};

} // namespace dynamic
