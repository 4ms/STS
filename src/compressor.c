/*
 * compressor.c - Basic soft limiting
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * Original idea for algoritm from https://www.kvraudio.com/forum/viewtopic.php?t=195315
 * Optimized and translated for int32_t by Dan Green
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "compressor.h"
#include "globals.h"

float MAX_SAMPLEVAL;
float THRESHOLD_COMPILED;
int32_t THRESHOLD_VALUE;

void init_compressor(uint32_t max_sample_val, float threshold_percent) {
	MAX_SAMPLEVAL = max_sample_val;

	float m = (float)max_sample_val;
	THRESHOLD_COMPILED = m * m * threshold_percent * (1.0 - threshold_percent);

	THRESHOLD_VALUE = threshold_percent * max_sample_val;
}

extern inline int32_t compress(int32_t val);

inline int32_t compress(int32_t val) {
	float tv = THRESHOLD_COMPILED / ((float)val);
	if (val > THRESHOLD_VALUE)
		return (MAX_SAMPLEVAL - tv);
	else if (val < -THRESHOLD_VALUE)
		return (-MAX_SAMPLEVAL - tv);
	else
		return val;
}
