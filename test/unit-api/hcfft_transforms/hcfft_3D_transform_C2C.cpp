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

#include "include/hcfft.h"
#include "../gtest/gtest.h"
#include <fftw3.h>
#include <hc_am.hpp>
#include "include/hcfftlib.h"
#include "./helper_functions.h"

TEST(hcfft_3D_transform_test, func_correct_3D_transform_C2C) {
  size_t N1, N2, N3;
  N1 = my_argc > 1 ? atoi(my_argv[1]) : 2;
  N2 = my_argc > 2 ? atoi(my_argv[2]) : 2;
  N3 = my_argc > 3 ? atoi(my_argv[3]) : 2;
  hcfftHandle plan;
  hcfftResult status = hcfftPlan3d(&plan, N1, N2, N3, HCFFT_C2C);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  int hSize = N1 * N2 * N3;
  hcfftComplex* input = (hcfftComplex*)malloc(hSize * sizeof(hcfftComplex));
  hcfftComplex* output = (hcfftComplex*)malloc(hSize * sizeof(hcfftComplex));

  // Populate the input
  for (int i = 0; i < hSize; i++) {
    input[i].x = i % 8;
    input[i].y = i % 16;
  }

  std::vector<hc::accelerator> accs = hc::accelerator::get_all();
  assert(accs.size() && "Number of Accelerators == 0!");
  hc::accelerator_view accl_view = accs[1].get_default_view();
  hcfftComplex* idata = hc::am_alloc(hSize * sizeof(hcfftComplex), accs[1], 0);
  accl_view.copy(input, idata, sizeof(hcfftComplex) * hSize);
  hcfftComplex* odata = hc::am_alloc(hSize * sizeof(hcfftComplex), accs[1], 0);
  accl_view.copy(output, odata, sizeof(hcfftComplex) * hSize);
  status = hcfftExecC2C(plan, idata, odata, HCFFT_FORWARD);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  accl_view.copy(odata, output, sizeof(hcfftComplex) * hSize);
  status = hcfftDestroy(plan);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  // FFTW work flow
  // input output arrays
  fftwf_complex *fftw_in, *fftw_out;
  fftwf_plan p;
  fftw_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * hSize);
  fftw_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * hSize);
  // Populate inputs
  for (int i = 0; i < hSize; i++) {
    fftw_in[i][0] = input[i].x;
    fftw_in[i][1] = input[i].y;
  }
  // 3D forward plan
  p = fftwf_plan_dft_3d(N3, N2, N1, fftw_in, fftw_out, FFTW_FORWARD,
                        FFTW_ESTIMATE);
  // Execute C2R
  fftwf_execute(p);
  // Check RMSE: If fails go for pointwise comparison
  if (JudgeRMSEAccuracyComplex<fftwf_complex, hcfftComplex>(fftw_out, output,
                                                            hSize)) {
    // Check Real Outputs
    for (int i = 0; i < hSize; i++) {
      EXPECT_NEAR(fftw_out[i][0], output[i].x, 0.1);
    }
    // Check Imaginary Outputs
    for (int i = 0; i < hSize; i++) {
      EXPECT_NEAR(fftw_out[i][1], output[i].y, 0.1);
    }
  }
  // Free up resources
  fftwf_destroy_plan(p);
  fftwf_free(fftw_in);
  fftwf_free(fftw_out);
  free(input);
  free(output);
  hc::am_free(idata);
  hc::am_free(odata);
}
