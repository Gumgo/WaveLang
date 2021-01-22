import array;
import core;
import constants;
import json;
import math;
@import sampler;

public const? real frequency_to_speed(in const? real root_frequency, in const? real target_frequency) {
	// root_frequency will likely be constant, so the division will be optimized away
	return target_frequency * (1 / root_frequency);
}

public const? real note_to_frequency(in const? real note) {
	// Note 69 corresponds to A440
	const? real note_offset = note - 69;

	// Calculate offset from A440 in octaves
	const? real octave_offset = note_offset * (1/12);

	return 440 * math.pow(2, octave_offset);
}

// Calculates the integral int{lower:upper} (1 - e^(ax + b)) * sin(cx) dx
public const? real integrate_distorted_line(in const? real a, in const? real b, in const? real c, in const? real lower, in const? real upper) {
	return integrate_distorted_line_from_zero(a, b, c, upper) - integrate_distorted_line_from_zero(a, b, c, lower);
}

public const? real integrate_distorted_line_from_zero(in const? real a, in const? real b, in const? real c, in const? real r) {
	return (1 - math.cos(r*c))/c - math.exp(b) * (a*math.exp(a*r)*math.sin(r*c) - c*math.exp(a*r)*math.cos(r*c) + c) / (a*a + c*c);
}

public real sampler_sin(in real frequency) {
	const real[] frequency_weights = json.read_real_array("common_waveforms.json", "sine");
	return sampler.sampler_wavetable(frequency_weights, frequency, 0);
}

public real sampler_sawtooth_phase(in real frequency, in real phase) {
	const real[] frequency_weights = json.read_real_array("common_waveforms.json", "sawtooth");
	return sampler.sampler_wavetable(frequency_weights, frequency, phase);
}

public real sampler_sawtooth(in real frequency) {
	return sampler_sawtooth_phase(frequency, 0);
}

public real sampler_sawtooth_distorted_phase(in real frequency, in const real distortion, in real phase) {
	const real harmonic_count = 1024;
	const real[] frequency_weights = {[0]} * harmonic_count;
	for (const real i : array.range(harmonic_count)) {
		// For distortion, we use the wavemapping function g(t) = sign(t) * (1 - e^-abs(d*t)), where d = distortion
		// A sawtooth wave is defined by the following piecewise function in the range [0,1):
		// f(x) = 2x-1
		// The nth fourier coefficient is given by 2*int_0^1 h(t)*sin(2*pi*n*t) dt
		// In our case, h(t) = g(f(t))

		// To solve, we break our integral into two parts to deal with the sign and abs functions:
		// int_0^0.5 -(1 - e^(-d*(-2t+1))) * sin(2*pi*n*t) dt
		// int_0.5^1 (1 - e^(-d*(2t-1))) * sin(2*pi*n*t) dt
		// All of these integrals take the form:
		// s * int (1 - e^(at + b)) * sin(ct) dt
		// with constants:
		// s = -1, a = 2d, b = -d, c = 2*pi*n
		// s = 1, a = -2d, b = d, c = 2*pi*n

		const real c = 2 * constants.k_pi * (i + 1);
		const real d = distortion;
		const real integral_a = -integrate_distorted_line(2*d, -d, c, 0, 0.5);
		const real integral_b = integrate_distorted_line(-2*d, d, c, 0.5, 1);

		const real weight = 2 * (integral_a + integral_b);
		frequency_weights[i] = weight;
	}
	return sampler.sampler_wavetable(frequency_weights, frequency, phase);
}

public real sampler_sawtooth_distorted(in real frequency, in const real distortion) {
	return sampler_sawtooth_distorted_phase(frequency, distortion, 0);
}

public real sampler_triangle(in real frequency) {
	const real[] frequency_weights = json.read_real_array("common_waveforms.json", "triangle");
	return sampler.sampler_wavetable(frequency_weights, frequency, 0);
}

public real sampler_pulse(in real frequency, in real phase) {
	real a = sampler_sawtooth_phase(frequency, 0);
	real b = sampler_sawtooth_phase(frequency, phase);
	return a - b + (2 * phase) - 1;
}

public real sampler_square(in real frequency) {
	return sampler_pulse(frequency, 0.5);
}

public real sampler_ext(in string sample, in const real channel, in const bool loop, in const bool bidi, in real speed) {
	return core.select(
		loop,
		sampler.sampler_loop(sample, channel, bidi, speed, 0),
		sampler.sampler(sample, channel, speed));
}