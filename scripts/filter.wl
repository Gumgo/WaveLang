import constants;
@import filter;
import math;
import stream;

// Biquad filter algorithms taken from here: http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt

public real filter_biquad_lowpass(in real cutoff_frequency, in real resonance, in real signal) {
	const real sample_rate = stream.get_sample_rate();

	real w0 = cutoff_frequency * ((2 * constants.k_pi) / sample_rate);

	real sin_w0;
	real cos_w0;
	math.sincos(w0, out sin_w0, out cos_w0);

	real alpha = sin_w0 * (2 / resonance);

	real a0 = 1 + alpha;
	real a1 = -2 * cos_w0;
	real a2 = 1 - alpha;

	real b1 = 1 - cos_w0;
	real b0 = b1 * 0.5;
	real b2 = b0;

	real inv_a0 = 1 / a0;

	return filter.biquad(
		a1 * inv_a0,
		a2 * inv_a0,
		b0 * inv_a0,
		b1 * inv_a0,
		b2 * inv_a0,
		signal);
}

public real filter_biquad_highpass(in real cutoff_frequency, in real resonance, in real signal) {
	const real sample_rate = stream.get_sample_rate();

	real w0 = cutoff_frequency * ((2 * constants.k_pi) / sample_rate);

	real sin_w0;
	real cos_w0;
	math.sincos(w0, out sin_w0, out cos_w0);

	real alpha = sin_w0 * (2 / resonance);

	real a0 = 1 + alpha;
	real a1 = -2 * cos_w0;
	real a2 = 1 - alpha;

	real b1 = -1 - cos_w0;
	real b0 = b1 * -0.5;
	real b2 = b0;

	real inv_a0 = 1 / a0;

	return filter.biquad(
		a1 * inv_a0,
		a2 * inv_a0,
		b0 * inv_a0,
		b1 * inv_a0,
		b2 * inv_a0,
		signal);
}

public real filter_biquad_bandpass(in real peak_frequency, in real peak_gain, in real signal) {
	const real sample_rate = stream.get_sample_rate();

	real w0 = peak_frequency * ((2 * constants.k_pi) / sample_rate);

	real sin_w0;
	real cos_w0;
	math.sincos(w0, out sin_w0, out cos_w0);

	real alpha = sin_w0 * (2 / peak_gain);

	real a0 = 1 + alpha;
	real a1 = -2 * cos_w0;
	real a2 = 1 - alpha;

	real b0 = 2 * sin_w0;
	real b1 = 0;
	real b2 = -b0;

	real inv_a0 = 1 / a0;

	return filter.biquad(
		a1 * inv_a0,
		a2 * inv_a0,
		b0 * inv_a0,
		b1 * inv_a0,
		b2 * inv_a0,
		signal);
}