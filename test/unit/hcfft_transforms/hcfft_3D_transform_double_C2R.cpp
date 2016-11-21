#include "hcfft.h"
#include "../gtest/gtest.h"
#include "fftw3.h"

TEST(hcfft_3D_transform_test, func_correct_3D_transform_Z2D ) {
  size_t N1, N2, N3;
  N1 = my_argc > 1 ? atoi(my_argv[1]) : 4;
  N2 = my_argc > 2 ? atoi(my_argv[2]) : 4;
  N3 = my_argc > 3 ? atoi(my_argv[3]) : 4;
  hcfftHandle* plan = NULL;
  hcfftResult status  = hcfftPlan3d(plan, N1, N2, N3, HCFFT_Z2D);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  int Csize = N3 * N2 * (1 + N1 / 2);
  int Rsize = N1 * N2 * N3;
  hcfftDoubleComplex* input = (hcfftDoubleComplex*)malloc(Csize * sizeof(hcfftDoubleComplex));
  hcfftDoubleReal* output = (hcfftDoubleReal*)malloc(Rsize * sizeof(hcfftDoubleReal));
  int seed = 123456789;
  srand(seed);

  // Populate the input
  for(int i = 0; i < Csize ; i++) {
    input[i].x = i%8;
    input[i].y = i%16;
  }

  std::vector<hc::accelerator> accs = hc::accelerator::get_all();
  assert(accs.size() && "Number of Accelerators == 0!");
  hc::accelerator_view accl_view = accs[1].get_default_view();
  hcfftDoubleComplex* idata = hc::am_alloc(Csize * sizeof(hcfftDoubleComplex), accs[1], 0);
  accl_view.copy(input, idata, sizeof(hcfftDoubleComplex) * Csize);
  hcfftDoubleReal* odata = hc::am_alloc(Rsize * sizeof(hcfftDoubleReal), accs[1], 0);
  accl_view.copy(output, odata, sizeof(hcfftDoubleReal) * Rsize);
  status = hcfftExecZ2D(*plan, idata, odata);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  accl_view.copy(odata, output, sizeof(hcfftDoubleReal) * Rsize);
  status =  hcfftDestroy(*plan);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  //FFTW work flow
  // input output arrays
  double *fftw_out; fftw_complex* fftw_in;
  fftw_plan p;
  fftw_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * Csize);
  // Populate inputs
  for(int i = 0; i < Csize ; i++) {
    fftw_in[i][0] = input[i].x;
    fftw_in[i][1] = input[i].y;
  }
  fftw_out = (double*) fftw_malloc(sizeof(double) * Rsize);
  // 3D forward plan
  p = fftw_plan_dft_c2r_3d(N3, N2, N1, fftw_in, fftw_out, FFTW_ESTIMATE | FFTW_HC2R);;
  // Execute C2R
  fftw_execute(p);
  //Check Real Outputs
  for (int i =0; i < Rsize; i++) {
    EXPECT_NEAR(fftw_out[i] , output[i], 0.1); 
  }
  // Free up resources
  fftw_destroy_plan(p);
  fftw_free(fftw_in); fftw_free(fftw_out); 
  free( input );
  free( output );
  hc::am_free(idata);
  hc::am_free(odata);
}

