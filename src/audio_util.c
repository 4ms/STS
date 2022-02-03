/*
 * audio_util.c - utilities for audio math
 *
 * Author: Dan Green (danngreen1@gmail.com)
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

#include "audio_util.h"
#include "globals.h"

uint32_t align_addr(uint32_t addr, uint32_t blockAlign) {
	volatile uint32_t t;

	if (blockAlign == 4)
		addr &= 0xFFFFFFFC;
	else if (blockAlign == 2)
		addr &= 0xFFFFFFF8; //was E? but it clicks if we align to 2 not 4, even if our file claims blockAlign = 2
	else if (blockAlign == 8)
		addr &= 0xFFFFFFF8;
	else if (blockAlign == 6) {
		t = addr / 6UL;
		addr = t * 6UL;
	} else if (blockAlign == 3) {
		t = addr / 3UL;
		addr = t * 3UL;
	} else if (blockAlign == 1)
		return addr;
	else
		return 0; //unknown block Align

	return addr;
}
