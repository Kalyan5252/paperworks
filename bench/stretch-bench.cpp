#include "signalsmith-stretch/signalsmith-stretch.h"
#include <chrono>
#include <cstdio>
#include <cmath>
#include <vector>
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;

int main() {
	const int sr = 48000, channels = 2, seconds = 30;
	const int len = sr*seconds;
	std::vector<std::vector<float>> in(channels, std::vector<float>(len));
	std::mt19937 rng(1);
	std::uniform_real_distribution<float> noise(-0.05f, 0.05f);
	for (int c = 0; c < channels; ++c) for (int i = 0; i < len; ++i) {
		float t = i/float(sr);
		float s = 0;
		for (int h = 1; h <= 12; ++h) s += std::sin(2*M_PI*220*h*t + c)/h*0.2f;
		in[c][i] = s + noise(rng);
	}
	struct Case { const char *name; bool cheaper; bool split; double time; double semis; double formant; };
	Case cases[] = {
		{"default  time 1.0  pitch 0 ", false, false, 1.0, 0, 0},
		{"default  time 1.25 pitch 0 ", false, false, 1.25, 0, 0},
		{"default  time 1.0  pitch +5", false, false, 1.0, 5, 0},
		{"default  time 1.25 pitch +5", false, false, 1.25, 5, 0},
		{"default  +5 +formant comp  ", false, false, 1.0, 5, 1},
		{"default  split, t1.25 p+5  ", false, true, 1.25, 5, 0},
		{"cheaper  time 1.25 pitch +5", true, true, 1.25, 5, 0},
	};
	const int outBlock = 256;
	for (auto &cs : cases) {
		Stretch s(42);
		if (cs.cheaper) s.presetCheaper(channels, sr, cs.split); else s.presetDefault(channels, sr, cs.split);
		s.setTransposeSemitones(cs.semis, cs.semis != 0 ? 8000.0f/sr : 0);
		if (cs.formant) s.setFormantFactor(1, true), s.setFormantBase(200.0f/sr);
		std::vector<std::vector<float>> out(channels, std::vector<float>(outBlock));
		std::vector<float*> ip(channels), op(channels);
		double inPos = 0; double rate = 1/cs.time;
		double worst = 0; long blocks = 0; double total = 0;
		while (inPos + outBlock*rate + 1 < len) {
			int i0 = int(inPos); inPos += outBlock*rate; int i1 = int(inPos);
			for (int c = 0; c < channels; ++c) { ip[c] = in[c].data() + i0; op[c] = out[c].data(); }
			auto t0 = std::chrono::steady_clock::now();
			s.process(ip, i1 - i0, op, outBlock);
			double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			total += dt; worst = std::max(worst, dt); ++blocks;
		}
		double audioSec = blocks*outBlock/double(sr);
		double blockBudget = outBlock/double(sr);
		std::printf("%s | block %5d int %5d | x%6.1f realtime | avg CPU %5.2f%% | worst 256-sample callback %6.3f ms (%5.1f%% of %.2f ms budget)\n",
			cs.name, s.blockSamples(), s.intervalSamples(), audioSec/total, 100*total/audioSec, worst*1000, 100*worst/blockBudget, blockBudget*1000);
	}
}
