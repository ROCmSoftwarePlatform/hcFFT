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
#pragma once

#include <hip_runtime_api.h>
#include <hcfft.h>

#ifdef   cplusplus
extern "C" {
#endif

typedef hcfftHandle hipfftHandle;
typedef void *hipStream_t;
typedef hcfftComplex hipfftComplex;
typedef hcfftDoubleComplex hipfftDoubleComplex;
typedef hcfftReal  hipfftReal;
typedef hcfftDoubleReal hipfftDoubleReal;

inline static hipfftResult hipHCFFTResultToHIPFFTResult(hcfftResult hcResult) 
{
   switch(hcResult) 
   {
    case HCFFT_SUCCESS:
        return HIPFFT_SUCCESS;
    case HCFFT_INVALID_PLAN:
        return HIPFFT_INVALID_PLAN;
    case HCFFT_ALLOC_FAILED:
        return HIPFFT_ALLOC_FAILED;
    case HCFFT_INVALID_TYPE:
        return HIPFFT_INVALID_TYPE;
    case HCFFT_INVALID_VALUE:
        return HIPFFT_INVALID_VALUE;
    case HCFFT_INTERNAL_ERROR:
        return HIPFFT_INTERNAL_ERROR;
    case HCFFT_EXEC_FAILED:
        return HIPFFT_EXEC_FAILED;
    case HCFFT_SETUP_FAILED:
        return HIPFFT_SETUP_FAILED;
    case HCFFT_INVALID_SIZE:
        return HIPFFT_INVALID_SIZE;
    case HCFFT_UNALIGNED_DATA:
        return HIPFFT_UNALIGNED_DATA;
    case HCFFT_INCOMPLETE_PARAMETER_LIST:
        return HIPFFT_INCOMPLETE_PARAMETER_LIST;
    case HCFFT_INVALID_DEVICE:
        return HIPFFT_INVALID_DEVICE;
    case HCFFT_PARSE_ERROR:
        return HIPFFT_PARSE_ERROR;
    case HCFFT_NO_WORKSPACE:
        return HIPFFT_NO_WORKSPACE;
    default:
         throw "Unimplemented Result";
   }
}

inline static hipfftType hipHIPFFTTypeToHCFFTType(hipfftType hipType) 
{
   switch(hipType) 
   {
    case HIPFFT_R2C:
        return HCFFT_R2C;
    case HIPFFT_C2R:
        return HCFFT_C2R
    case HIPFFT_C2C:
        return HCFFT_C2C;
    case HIPFFT_D2Z:
        return HCFFT_D2Z;
    case HIPFFT_Z2D:
        return HCFFT_Z2D;
    case HIPFFT_Z2Z:
        return HCFFT_Z2Z;
    default:
        throw "Unimplemented Type";
  }
}


inline static hipfftResult hipfftCreate(hipfftHandle *plan){
    return hipHCFFTResultToHIPFFTResult(hcfftCreate(plan));
}

inline static hipfftResult hipfftSetStream(hipfftHandle plan, hipStream_t stream){

    accelerator acc = accelerator(accelerator::default_accelerator);
    accelerator_view accl_view = acc.default_view;

    return hipHCFFTResultToHIPFFTResult(hcfftSetStream(plan, &accl_view));
}

/*hipFFT Basic Plans*/

inline static hipfftResult hipfftPlan1d(hipfftHandle *plan, int nx, hipfftType type, int batch){
    return hipHCFFTResultToHIPFFTResult(hcfftPlan1d(plan, nx, hipHIPFFTTypeToHCFFTType(type), batch));
}

inline static hipfftResult hipfftPlan2d(hipfftHandle *plan, int nx, int ny, hipfftType type){
    return hipHCFFTResultToHIPFFTResult(hcfftPlan2d(plan, nx, ny, hipHIPFFTTypeToHCFFTType(type)));
}

inline static hipfftResult hipfftPlan3d(hipfftHandle *plan, int nx, int ny, int nz, hipfftType type){
    return hipHCFFTResultToHIPFFTResult(hcfftPlan3d(plan, nx, ny, nz, hipHIPFFTTypeToHCFFTType(type)));
}

inline static hipfftResult hipfftDestroy(hipfftHandle plan){
    return hipHCFFTResultToHIPFFTResult(hcfftDestroy(plan));
}

/*hipFFT Execution*/

inline static hipfftResult hipfftExecC2C(hipfftHandle plan, hipfftComplex *idata, 
                                         hipfftComplex *odata, int direction){
    return hipHCFFTResultToHIPFFTResult(hcfftExecC2C(plan, idata, odata, direction));
}

inline static hipfftResult hipfftExecZ2Z(hipfftHandle plan, hipfftDoubleComplex *idata, 
                                         hipfftDoubleComplex *odata, int direction){
    return hipHCFFTResultToHIPFFTResult(hcfftExecZ2Z(plan, idata, odata, direction));
}

inline static hipfftResult hipfftExecR2C(hipfftHandle plan, hipfftReal *idata, 
                                         hipfftComplex *odata){
    return hipHCFFTResultToHIPFFTResult(hcfftExecR2C(plan, idata, odata));
}

inline static hipfftResult hipfftExecD2Z(hipfftHandle plan, hipfftDoubleReal *idata, 
                                         hipfftDoubleComplex *odata){
    return hipHCFFTResultToHIPFFTResult(hcfftExecD2Z(plan, idata, odata));
}

inline static hipfftResult hipfftExecC2R(hipfftHandle plan, hipfftComplex *idata, 
                                         hipfftReal *odata){
    return hipHCFFTResultToHIPFFTResult(hcfftExecC2R(plan, idata, odata));
}

inline static hipfftResult hipfftExecZ2D(hipfftHandle plan, hipfftComplex *idata, 
                                         hipfftReal *odata){
    return hipHCFFTResultToHIPFFTResult(hcfftExecZ2D(plan, idata, odata));
}


#ifdef __cplusplus
}
#endif


