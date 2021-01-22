import math;

public real adsr_linear(
	in real attack_duration,
	in real decay_duration,
	in real sustain_level,
	in real release_duration,
	in real note_press_duration,
	in real note_release_duration) {
	real attack_ratio = note_press_duration / attack_duration;
	attack_ratio = math.min(math.max(attack_ratio, 0), 1);

	real decay_start = attack_duration;
	real decay_ratio = (note_press_duration - decay_start) / decay_duration;
	decay_ratio = math.min(math.max(decay_ratio, 0), 1);

	real sustain_start = decay_start + decay_duration;
	real note_release_time = note_press_duration - note_release_duration;
	real note_release_early_duration = math.max(0, sustain_start - note_release_time);
	real release_ratio = (note_release_duration - note_release_early_duration) / release_duration;
	release_ratio = math.min(math.max(release_ratio, 0), 1);

	return (attack_ratio + decay_ratio * (sustain_level - 1)) * (1 - release_ratio);
}