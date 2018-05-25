/*
 * compressor.h - Basic soft limiting
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

#pragma once

#include <stm32f4xx.h>

#define C_THRESHOLD_75_16BIT 201326592 /*  ((1<<15)*(1<<15)*0.75*0.75 - 0.75*(1<<15)*(1<<15))  */
#define C_THRESHOLD_75_32BIT 864691128455135232

/*  ((1<<31)*(1<<31)*0.75*0.75 - 0.75*(1<<31)*(1<<31))
(1<<58 * 9) - (1<<60 * 3)
2594073385365405696 - 3458764513820540928
=-864691128455135232
*/


void init_compressor(uint32_t max_sample_val, float threshold_percent);


