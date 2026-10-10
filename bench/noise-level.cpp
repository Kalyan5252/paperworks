// Checks the "consistency" claim from Juillerat & Hirsbrunner (ICASSP 2017, §5):
// a phase vocoder keeps the level of tonal signals but can lose up to 3 dB on noise.
// Measures the output/input RMS ratio of Signalsmith Stretch for white noise and a sine.
#include "signalsmith-stretch/signalsmith-stretch.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;

static double rms(const std::vector<float> &x, size_t from, size_t to) {
	double sum = 0;
	for (size_t i = from; i < to; ++i) sum += double(x[i])*x[i];
	return std::sqrt(sum/(to - from));
}

int main() {
	const int sr = 48000, len = sr*10;
	std::vector<float> noise(len), sine(len);
	std::mt19937 rng(1);
	std::normal_distribution<float> gauss(0, 0.1f);
	for (int i = 0; i < len; ++i) {
		noise[i] = gauss(rng);
		sine[i] = 0.1f*std::sqrt(2.0f)*std::sin(2*M_PI*440.0*i/sr);
	}
	const double ratios[] = {0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};
	std::printf("ratio   noise dB   sine dB   noise-vs-sine dB\n");
	for (double ratio : ratios) {
		double db[2];
		const std::vector<float> *signals[2] = {&noise, &sine};
		for (int s = 0; s < 2; ++s) {
			Stretch stretch;
			stretch.presetDefault(1, sr);
			int outLen = int(len*ratio);
			std::vector<float> out(outLen);
			const float *inPtr[1] = {signals[s]->data()};
			float *outPtr[1] = {out.data()};
			stretch.exact(inPtr, len, outPtr, outLen);
			// ignore the first and last second (edge handling)
			double inRms = rms(*signals[s], sr, len - sr);
			double outRms = rms(out, size_t(sr*ratio), size_t(outLen - sr*ratio));
			db[s] = 20*std::log10(outRms/inRms);
		}
		std::printf("%5.2f   %+7.2f   %+7.2f   %+7.2f\n", ratio, db[0], db[1], db[0] - db[1]);
	}
}
