// Measures transient smearing of Signalsmith Stretch, the problem targeted by
// Juillerat & Hirsbrunner (ICASSP 2017). Input: short percussive bursts every 500 ms.
// For each burst in the output, reports pre-echo (energy before the onset) and the
// share of the burst's energy that lands within 10 ms after the expected onset.
// Run with "mixed" to add a steady sawtooth chord under the bursts; the burst part of
// the output's energy is then estimated as energy(stretch(bursts + chord)) - energy(stretch(chord)),
// which ignores harmless phase changes in the chord.
#include "signalsmith-stretch/signalsmith-stretch.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;

static double energy(const std::vector<float> &x, long from, long to) {
	double sum = 0;
	for (long i = std::max(0L, from); i < std::min(long(x.size()), to); ++i) sum += double(x[i])*x[i];
	return sum;
}

int main(int argc, char **argv) {
	bool mixed = argc > 1 && std::string(argv[1]) == "mixed";
	const int sr = 48000, len = sr*10, period = sr/2;
	std::vector<float> in(len, 0);
	std::mt19937 rng(1);
	std::normal_distribution<float> gauss(0, 0.3f);
	for (int start = period/2; start + period <= len; start += period) {
		for (int i = 0; i < sr/50; ++i) in[start + i] = gauss(rng)*std::exp(-i/(0.003*sr)); // 3 ms decay
	}
	std::vector<float> steady(len, 0);
	if (mixed) {
		const double notes[] = {110, 138.6, 164.8};
		for (int i = 0; i < len; ++i) {
			for (double f : notes) for (int h = 1; f*h < 12000; ++h) steady[i] += 0.03f*std::sin(2*M_PI*f*h*i/sr)/h;
		}
	}
	std::vector<float> mix(len);
	for (int i = 0; i < len; ++i) mix[i] = in[i] + steady[i];
	struct Config { const char *name; double blockMs, intervalMs; };
	const Config configs[] = {{"default 120/30 ms", 120, 30}, {"medium   24/6 ms ", 24, 6}, {"short     6/1.5ms", 6, 1.5}};
	const double ratios[] = {1.0, 1.5, 2.0};
	std::printf("config              ratio   pre-echo dB   energy in first 10 ms\n");
	for (auto &config : configs) {
		for (double ratio : ratios) {
			long outLen = long(len*ratio);
			auto render = [&](std::vector<float> &signal) {
				Stretch stretch;
				stretch.configure(1, int(config.blockMs*sr/1000), int(config.intervalMs*sr/1000));
				std::vector<float> out(outLen);
				const float *inPtr[1] = {signal.data()};
				float *outPtr[1] = {out.data()};
				stretch.exact(inPtr, len, outPtr, int(outLen));
				return out;
			};
			std::vector<float> out = render(mix), steadyOut;
			if (mixed) steadyOut = render(steady);
			auto burstEnergy = [&](long from, long to) {
				return mixed ? energy(out, from, to) - energy(steadyOut, from, to) : energy(out, from, to);
			};
			double pre = 0, early = 0, total = 0;
			for (int start = period/2 + period; start + 2*period <= len; start += period) { // skip first/last
				long onset = long(start*ratio);
				long ms = sr/1000;
				pre += burstEnergy(onset - 100*ms, onset - 1*ms);
				early += burstEnergy(onset - 1*ms, onset + 10*ms);
				total += burstEnergy(onset - 100*ms, onset + 200*ms);
			}
			pre = std::max(pre, 1e-12*total);
			std::printf("%s   %4.1f      %+7.1f          %5.1f %%\n", config.name, ratio,
				10*std::log10(pre/total), 100*early/total);
		}
	}
}
