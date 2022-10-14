#include "audio_util.h"
#include "circular_buffer_cache.h"
#include "globals.h"
#include "params.h"
#include "sample_file.h"
#include "sampler.h"

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

// Returns number of blocks that we should take to do a fast fade in perc mode
// Fast fades are PERC_FADEUP and REV_PERC_FADEDOWN
// and when we need to fade-down quickle because we're about to hit the end
static inline uint32_t calc_perc_fadedown_blocks(float length) {
	extern GlobalParams global_params;

	const float time = length * PERC_ENV_FACTOR / global_params.f_record_sample_rate;

	if (time <= 0.010f) //faster than 100Hz, 10ms --> 1 block (0.3ms at 44k)
		return 1;

	const uint32_t max_fadedown = (0.003f * global_params.f_record_sample_rate) / FramesPerBlock;

	if (time >= 0.100f) //slower than 10Hz, 100ms --> 3ms fade
		return max_fadedown;

	return ((time - 0.010f) / 0.100f) * (max_fadedown - 1) + 1;

	//TODO: compare performance of this vs. doing the interpolate and then limiting
	//TODO: compare performance of this vs. pre-calculating PERC_ENV_FACTOR/sr and max_fadedown whenever sr changes
	//44100Hz --> 3ms is 8.2 blocks, so return 9 blocks
}

static inline float calc_fast_perc_fade_rate(float length) {
	return 1.f / (calc_perc_fadedown_blocks(length) * FramesPerBlock);
}

// calc_resampled_cache_size()
// Amount an imaginary pointer in the sample file would move with each audio block sent to the codec
static inline uint32_t calc_resampled_cache_size(uint8_t samplenum, uint8_t banknum, uint32_t resampled_buffer_size) {
	return ((resampled_buffer_size * samples[banknum][samplenum].sampleByteSize) / 2);
}

// calc_resampled_buffer_size()
// Amount play_buff[]->out changes with each audio block sent to the codec
static inline uint32_t calc_resampled_buffer_size(uint8_t samplenum, uint8_t banknum, float resample_rate) {
	return ((uint32_t)((HT16_CHAN_BUFF_LEN * samples[banknum][samplenum].numChannels * 2) * resample_rate));
}

// calc_dist_to_end()
// How many samples left to go before we hit the stopping point
static inline int32_t calc_dist_to_end(uint8_t chan, uint8_t samplenum, uint8_t banknum) {
	extern uint32_t sample_file_endpos[NUM_PLAY_CHAN];
	extern CircularBuffer *play_buff[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
	extern uint32_t cache_low[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
	extern uint32_t cache_map_pt[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
	extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

	uint32_t sample_file_playpos;

	//Find out where the audio output data is relative to the start of the cache
	sample_file_playpos = map_buffer_to_cache(play_buff[chan][samplenum]->out,
											  samples[banknum][samplenum].sampleByteSize,
											  cache_low[chan][samplenum],
											  cache_map_pt[chan][samplenum],
											  play_buff[chan][samplenum]);

	//Calculate the distance left to the end that we should be playing
	//TODO: check if playpos is in bounds of startpos as well
	if (!i_param[chan][REV])
		return (sample_file_endpos[chan] > sample_file_playpos) ? (sample_file_endpos[chan] - sample_file_playpos) : 0;
	else
		return (sample_file_playpos > sample_file_endpos[chan]) ? (sample_file_playpos - sample_file_endpos[chan]) : 0;
}

// calc_start_point()
static uint32_t calc_start_point(float start_param, Sample *sample) {
	uint32_t zeropt;
	uint32_t inst_size;

	zeropt = sample->inst_start;
	inst_size = sample->inst_end - sample->inst_start;

	//If the sample size is smaller than two blocks, the start point is forced to the start
	if (inst_size <= (READ_BLOCK_SIZE * 2))
		return (align_addr(zeropt, sample->blockAlign));

	if (start_param < 0.002f)
		return (align_addr(zeropt, sample->blockAlign));
	else if (start_param > 0.998f)
		return (align_addr((zeropt + inst_size - (READ_BLOCK_SIZE * 2)),
						   sample->blockAlign)); //just play the last 32 blocks (~64k samples)
	else
		return (align_addr((zeropt + ((uint32_t)(start_param * (float)inst_size))), sample->blockAlign));
}

uint32_t ceil(float num) {
	uint32_t inum = (uint32_t)num;
	if (num == (float)inum) {
		return inum;
	}
	return inum + 1;
}

// calc_stop_point()
// Returns an offset from the startpos, based on the length param (knob and CV) and resampling rate
//
static uint32_t calc_stop_point(float length_param, float resample_param, Sample *sample, uint32_t startpos) {
	uint32_t fwd_stop_point;
	uint32_t num_samples_to_play;
	uint32_t max_play_length;
	float seconds;
	float t_f;
	uint32_t t_int;

	seconds = (float)(sample->sampleRate * sample->blockAlign);
	max_play_length = sample->inst_end - sample->inst_start;
	// as opposed to taking sample->inst_size because that won't be clipped to the end of a sample file

	// 100% >= knob > 98% -->  play 100%
	if (length_param > 0.98f) {
		num_samples_to_play = max_play_length;
	}

	// 98% >= knob >= 50% and sample length <= 0.625s
	else if (length_param > 0.5f && max_play_length <= (0.625f * seconds))
	{
		const float half_sec = 0.5f * seconds;
		// --- 0 < sample length <= 0.5s ---> play full sample length
		if (max_play_length <= half_sec)
			num_samples_to_play = max_play_length;
		else
			// --- 0.5s < sample length <= 0.625 ---> play between 0.5s and full sample
			num_samples_to_play = half_sec + ((max_play_length - half_sec) * (length_param - 0.5f) / (0.98f - 0.5f));
	}

	// 98% >= knob > 95%  --> play between 100% to 80% of full sample
	else if (length_param > 0.95f)
	{
		num_samples_to_play = max_play_length * (6.67f * length_param - 5.5366f);
	}

	// 95% >= knob > 50%  (and sample length > 0.625s)
	else if (length_param > 0.50f)
	{
		// --- sample length > 5s
		if (max_play_length > (5.0f * seconds)) {
			if (length_param > 0.85f)
				// -------- 95% >= knob > 85%  --> play 80%  to 5.5s
				num_samples_to_play =
					4.5f * seconds + ((length_param - 0.85f) * 8.f) * (max_play_length - 4.5f * seconds);
			else
				// -------- 85% >= knob > 50%  --> play 4.5s to 0.5s
				num_samples_to_play = (11.43f * length_param - 5.215f) * seconds;
		}

		// --- 5s >= sample length > 0.625s --> play 0.5s to 80%
		else
		{
			num_samples_to_play =
				0.5f * seconds + (length_param * 1.7778f - 0.8889f) * (max_play_length - 0.625f * seconds);
		}
	}

	// length_param <= 50%: fixed envelope
	else
	{
		//number of sample frames to play is length*PERC_ENV_FACTOR plus the fadedown env
		//rounded up to multiples of FramesPerBlock
		//times the playback resample rate (ratio of wavfile SR : playback SR)
		//times the block align gives us how much the buffer address will increment
		t_f = (length_param * PERC_ENV_FACTOR) + calc_perc_fadedown_blocks(length_param);

		//round up to nearest audio block size
		t_f = ceil(t_f / FramesPerBlock) * FramesPerBlock;

		t_f *= resample_param;
		t_int = ((uint32_t)t_f) * sample->blockAlign;
		num_samples_to_play = t_int;
	}

	fwd_stop_point = startpos + num_samples_to_play;

	if (fwd_stop_point > sample->inst_end)
		fwd_stop_point = sample->inst_end;

	return (align_addr(fwd_stop_point, sample->blockAlign));
}
