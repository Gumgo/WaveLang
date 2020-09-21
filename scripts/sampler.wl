#import "constants.wl"

module real frequency_to_speed(in real root_frequency, in real target_frequency) {
	// root_frequency will likely be constant, so the division will be optimized away
	return target_frequency * (1 / root_frequency);
}

module real note_to_frequency(in real note) {
	// Note 69 corresponds to A440
	real note_offset := note - 69;

	// Calculate offset from A440 in octaves
	real octave_offset := note_offset * (1/12);

	return 440 * math.pow(2, octave_offset);
}

// Calculates the integral int{lower:upper} (1 - e^(ax + b)) * sin(cx) dx
module real integrate_distorted_line(in real a, in real b, in real c, in real lower, in real upper) {
	return integrate_distorted_line_from_zero(a, b, c, upper) - integrate_distorted_line_from_zero(a, b, c, lower);
}

module real integrate_distorted_line_from_zero(in real a, in real b, in real c, in real r) {
	return (1 - math.cos(r*c))/c - math.exp(b) * (a*math.exp(a*r)*math.sin(r*c) - c*math.exp(a*r)*math.cos(r*c) + c) / (a*a + c*c);
}

// MEGA HACK: currently the language is too slow to handle huge arrays required for wavetables, so the following wavetables are built into the runtime:
// "__native_sin", "__native_sawtooth", "__native_triangle"
// When using these, frequency is used directly as speed (i.e. the base frequency is 1)

module real sampler_sin(in real frequency) {
	//real[] frequency_weights := real[](1);
	//return sampler.sampler_wavetable(frequency_weights, frequency, 0);
	return sampler.sampler_loop("__native_sin", 0, false, frequency, 0);
}

module real sampler_sawtooth_phase(in real frequency, in real phase) {
	//real[] frequency_weights := real[]();
	//real k := 1;
	//repeat (1024) {
	//	real weight := -2 / (pi() * k);
	//	frequency_weights := frequency_weights + real[](weight);
	//	k := k + 1;
	//}
	//return sampler.sampler_wavetable(frequency_weights, frequency, phase);
	return sampler.sampler_loop("__native_sawtooth", 0, false, frequency, phase);
}

module real sampler_sawtooth(in real frequency) {
	return sampler_sawtooth_phase(frequency, 0);
}

module real sampler_sawtooth_distorted_phase(in real frequency, in real distortion, in real phase) {
	real const_distortion := core.enforce_const(distortion);

	real[] frequency_weights := real[]();
	real k := 1;
	repeat (1024) {
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

		real c := 2*pi()*k;
		real d := const_distortion;
		real integral_a := -integrate_distorted_line(2*d, -d, c, 0, 0.5);
		real integral_b := integrate_distorted_line(-2*d, d, c, 0.5, 1);

		real weight := 2 * (integral_a + integral_b);
		frequency_weights := frequency_weights + real[](weight);
		k := k + 1;
	}
	return sampler.sampler_wavetable(frequency_weights, frequency, phase);
}

module real sampler_sawtooth_distorted(in real frequency, in real distortion) {
	return sampler_sawtooth_distorted_phase(frequency, distortion, 0);
}

module real sampler_triangle(in real frequency) {
	//real[] frequency_weights := real[]();
	//real k := 1;
	//repeat (1024) {
	//	real s := core.select(((k - 1) / 2) % 2 == 0, 1, -1);
	//	real pi_k := pi() * k;
	//	real weight := core.select(k % 2 == 0, 0, s * 8 / (pi_k * pi_k));
	//	frequency_weights := frequency_weights + real[](weight);
	//	k := k + 1;
	//}
	//return sampler.sampler_wavetable(frequency_weights, frequency, 0);
	return sampler.sampler_loop("__native_triangle", 0, false, frequency, 0);
}

module real sampler_pulse(in real frequency, in real phase) {
	real a := sampler_sawtooth_phase(frequency, 0);
	real b := sampler_sawtooth_phase(frequency, phase);
	return a - b + (2*phase) - 1;
}

module real sampler_square(in real frequency) {
	return sampler_pulse(frequency, 0.5);
}

module real sampler_ext(in string sample, in real channel, in bool loop, in bool bidi, in real speed) {
	bool loop_const := core.enforce_const(loop);
	return core.select(loop_const,
		sampler.sampler_loop(sample, channel, bidi, speed, 0),
		sampler.sampler(sample, channel, speed));
}