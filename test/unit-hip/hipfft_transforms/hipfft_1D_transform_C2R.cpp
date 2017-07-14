/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "include/hipfft.h"
#include "../gtest/gtest.h"
#include <fftw3.h>
#include "./helper_functions.h"
#include "hip/hip_runtime.h"

TEST(hipfft_1D_transform_test, func_correct_1D_transform_C2R) {
  size_t N1;
  N1 = my_argc > 1 ? atoi(my_argv[1]) : 1024;

  // HIPFFT work flow
  hipfftHandle plan;
  hipfftResult status = hipfftPlan1d(&plan, N1, HIPFFT_C2R, 1);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  int Csize = (N1 / 2) + 1;
  int Rsize = N1;
  hipfftComplex* input = (hipfftComplex*)calloc(Csize, sizeof(hipfftComplex));
  hipfftReal* output = (hipfftReal*)calloc(Rsize, sizeof(hipfftReal));

  // Populate the input
  for (int i = 0; i < Csize; i++) {
    input[i].x = i % 8;
    input[i].y = i % 16;
  }

  hipfftComplex* idata;
  hipfftReal* odata;
  hipMalloc((void**)&idata, Csize * sizeof(hipfftComplex));
  hipMemcpy(idata, input, sizeof(hipfftComplex) * Csize, hipMemcpyHostToDevice);
  hipMalloc((void**)&odata, Rsize * sizeof(hipfftReal));
  hipMemcpy(odata, output, sizeof(hipfftReal) * Rsize, hipMemcpyHostToDevice);
  status = hipfftExecC2R(plan, idata, odata);
  EXPECT_EQ(status, HIPFFT_SUCCESS);
  hipMemcpy(output, odata, sizeof(hipfftReal) * Rsize, hipMemcpyDeviceToHost);
  status = hipfftDestroy(plan);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  // FFTW work flow
  // input output arrays
  float* fftw_out;
  fftwf_complex* fftw_in;
  int lengths[1] = {Rsize};
  fftwf_plan p;
  fftw_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * Csize);
  fftw_out = (float*)fftwf_malloc(sizeof(float) * Rsize);

  // Populate inputs
  for (int i = 0; i < Csize; i++) {
    fftw_in[i][0] = input[i].x;
    fftw_in[i][1] = input[i].y;
  }

  // 1D forward plan
  p = fftwf_plan_many_dft_c2r(1, lengths, 1, fftw_in, NULL, 1, 0, fftw_out,
                              NULL, 1, 0, FFTW_ESTIMATE | FFTW_HC2R);

  // Execute C2R
  fftwf_execute(p);

  // Check RMSE : IF fails go for
  if (JudgeRMSEAccuracyReal<float, hipfftReal>(fftw_out, output, Rsize)) {
    // Check Real Outputs
    for (int i = 0; i < Rsize; i++) {
      EXPECT_NEAR(fftw_out[i], output[i], 0.1);
    }
  }

  // Free up resources
  fftwf_destroy_plan(p);
  fftwf_free(fftw_in);
  fftwf_free(fftw_out);
  free(input);
  free(output);
  hipFree(idata);
  hipFree(odata);
}
