/*
 * Copyright 2017, OYMotion Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EMGFILTERS_H
#define _EMGFILTERS_H

#define SAMPLE_FREQ_500HZ 500
#define SAMPLE_FREQ_1000HZ 1000

#define NOTCH_FREQ_50HZ 50
#define NOTCH_FREQ_60HZ 60

// EMGFilters provides an anti-hum notch filter, a lowpass filter,
// and a highpass filter. Only 500 Hz and 1000 Hz sample rates are supported.
class EMGFilters {
  public:
    void init(int sampleFreq,
              int notchFreq,
              bool enableNotchFilter = true,
              bool enableLowpassFilter = true,
              bool enableHighpassFilter = true);

    int update(int inputValue);

  private:
    int m_sampleFreq;
    int m_notchFreq;
    bool m_bypassEnabled;
    bool m_notchFilterEnabled;
    bool m_lowpassFilterEnabled;
    bool m_highpassFilterEnabled;
};

#endif
