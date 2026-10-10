// The other side of the time-frequency trade-off from Juillerat & Hirsbrunner (ICASSP 2017):
// short windows keep transients sharp but damage steady tones. Stretches a steady
// 110 Hz sawtooth and reports how much output energy lies more than 5 Hz away from
// its partials (modulation sidebands / roughness), as dB relative to the total.
#include "signalsmith-stretch/signalsmith-stretch.h"
#include <cmath>
#include <cstdio>
#include <vector>
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;

// |X[k]|^2 of DFT bin k (Goertzel), for an already-windowed segment
static double binPower(const std::vector<double> &x, int k) {
	double coeff = 2*std::cos(2*M_PI*k/x.size()), s1 = 0, s2 = 0;
	for (double v : x) {
		double s0 = v + coeff*s1 - s2;
		s2 = s1;
		s1 = s0;
	}
	return s1*s1 + s2*s2 - coeff*s1*s2;
}

int main() {
	const int sr = 48000, len = sr*6;
	const double f0 = 110;
	std::vector<float> in(len, 0);
	for (int i = 0; i < len; ++i) {
		for (int h = 1; f0*h < 12000; ++h) in[i] += 0.1f*std::sin(2*M_PI*f0*h*i/sr)/h;
	}
	struct Config { const char *name; double blockMs, intervalMs; };
	const Config configs[] = {{"default 120/30 ms", 120, 30}, {"medium   24/6 ms ", 24, 6}, {"short     6/1.5ms", 6, 1.5}};
	const double ratios[] = {1.0, 1.5, 2.0};
	std::printf("config              ratio   off-partial energy dB\n");
	for (auto &config : configs) {
		for (double ratio : ratios) {
			Stretch stretch;
			stretch.configure(1, int(config.blockMs*sr/1000), int(config.intervalMs*sr/1000));
			int outLen = int(len*ratio);
			std::vector<float> out(outLen);
			const float *inPtr[1] = {in.data()};
			float *outPtr[1] = {out.data()};
			stretch.exact(inPtr, len, outPtr, outLen);
			// 2-second segment from the middle of the output
			int n = 2*sr, from = outLen/2 - n/2;
			std::vector<double> seg(n);
			double total = 0, tonal = 0;
			for (int i = 0; i < n; ++i) {
				seg[i] = (0.5 - 0.5*std::cos(2*M_PI*i/n))*out[from + i];
				total += seg[i]*seg[i];
			}
			// Parseval: sum |X[k]|^2 over all N bins = N * sum x^2; count both +/- frequencies
			const int binsPerHz = n/sr, halfWidth = 5*binsPerHz;
			for (int h = 1; f0*h < 12000; ++h) {
				int centre = int(std::round(f0*h*binsPerHz));
				for (int k = centre - halfWidth; k <= centre + halfWidth; ++k) tonal += 2*binPower(seg, k)/n;
			}
			double residual = std::max(total - tonal, 1e-12*total);
			std::printf("%s   %4.1f      %+7.1f\n", config.name, ratio, 10*std::log10(residual/total));
		}
	}
}
