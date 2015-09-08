#import "utility.wl"

module real frequency_to_speed(in real root_frequency, in real target_frequency) {
	// root_frequency will likely be constant, so the division will be optimized away
	return target_frequency * (1 / root_frequency);
}

module real note_to_frequency(in real note) {
	// Note 69 corresponds to A440
	real note_offset := note - 69;

	// Calculate offset from A440 in octaves
	real octave_offset := note_offset * (1/12);

	return 440 * pow(2, octave_offset);
}

module real sampler_sin(in real frequency) {
	return __native_sampler("__native_sin", 0, true, false, frequency_to_speed(440, frequency));
}

module real sampler_sawtooth(in real frequency) {
	return __native_sampler("__native_sawtooth", 0, true, false, frequency_to_speed(440, frequency));
}

module real sampler_triangle(in real frequency) {
	return __native_sampler("__native_triangle", 0, true, false, frequency_to_speed(440, frequency));
}