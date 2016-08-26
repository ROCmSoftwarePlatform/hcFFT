#include <math.h>
#include "stockham.h"
#include <list>
// FFT Stockham Autosort Method
//
//   Each pass does one digit reverse in essence. Hence by the time all passes are done, complete
//   digit reversal is done and output FFT is in correct order. Intermediate FFTs are stored in natural order,
//   which is not the case with basic Cooley-Tukey algorithm. Natural order in intermediate data makes it
//   convenient for stitching together passes with different radices.
//
//  Basic FFT algorithm:
//
//        Pass loop
//        {
//            Outer loop
//            {
//                Inner loop
//                {
//                }
//            }
//        }
//
//  The sweeps of the outer and inner loop resemble matrix indexing, this matrix changes shape with every pass as noted below
//
//   FFT pass diagram (radix 2)
//
//                k            k+R                                    k
//            * * * * * * * * * * * * * * * *                     * * * * * * * *
//            *   |             |           *                     *   |         *
//            *   |             |           *                     *   |         *
//            *   |             |           * LS        -->       *   |         *
//            *   |             |           *                     *   |         *
//            *   |             |           *                     *   |         *
//            * * * * * * * * * * * * * * * *                     *   |         *
//                         RS                                     *   |         * L
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                * * * * * * * *
//                                                                       R
//
//
//    With every pass, the matrix doubles in height and halves in length
//
//
//  N = 2^T = Length of FFT
//  q = pass loop index
//  k = outer loop index = (0 ... R-1)
//  j = inner loop index = (0 ... LS-1)
//
//  Tables shows how values change as we go through the passes
//
//    q | LS   |  R   |  L  | RS
//   ___|______|______|_____|___
//    0 |  1   | N/2  |  2  | N
//    1 |  2   | N/4  |  4  | N/2
//    2 |  4   | N/8  |  8  | N/4
//    . |  .   | .    |  .  | .
//  T-1 |  N/2 | 1    |  N  | 2
//
//
//   Data Read Order
//     Radix 2: k*LS + j, (k+R)*LS + j
//     Radix 3: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j
//     Radix 4: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j, (k+3R)*LS + j
//     Radix 5: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j, (k+3R)*LS + j, (k+4R)*LS + j
//
//   Data Write Order
//       Radix 2: k*L + j, k*L + j + LS
//       Radix 3: k*L + j, k*L + j + LS, k*L + j + 2*LS
//       Radix 4: k*L + j, k*L + j + LS, k*L + j + 2*LS, k*L + j + 3*LS
//       Radix 5: k*L + j, k*L + j + LS, k*L + j + 2*LS, k*L + j + 3*LS, k*L + j + 4*LS
//


namespace StockhamGenerator {
// Experimental End ===========================================

#define RADIX_TABLE_COMMON                              {     2048,           256,             1,         4,    { 8, 8, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {      512,            64,             1,         3,    { 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {      256,            64,             1,         4,    { 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {       64,            64,             4,         3,    { 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {       32,            64,            16,         2,    { 8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {       16,            64,            16,         2,    { 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {        4,            64,            32,         2,    { 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },  \
                                                        {        2,            64,            64,         1,    { 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },


template <Precision PR>
class KernelCoreSpecs {
  struct SpecRecord {
    size_t length;
    size_t workGroupSize;
    size_t numTransforms;
    size_t numPasses;
    size_t radices[12]; // Setting upper limit of number of passes to 12
  };

  typedef typename std::map<size_t, SpecRecord> SpecTable;
  SpecTable specTable;

 public:
  KernelCoreSpecs() {
    switch(PR) {
      case P_SINGLE: {
          SpecRecord specRecord[] = {

            RADIX_TABLE_COMMON

            //  Length, WorkGroupSize, NumTransforms, NumPasses,  Radices
            {     4096,           256,             1,         4,     { 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0 } },
            {     1024,           128,             1,         4,     { 8, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 } },
            {      128,            64,             4,         3,     { 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
            {        8,            64,            32,         2,     { 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },

          };
          size_t tableLength = sizeof(specRecord) / sizeof(specRecord[0]);

          for(size_t i = 0; i < tableLength; i++) {
            specTable[specRecord[i].length] = specRecord[i];
          }
        }
        break;

      case P_DOUBLE: {
          SpecRecord specRecord[] = {

            RADIX_TABLE_COMMON

            //  Length, WorkGroupSize, NumTransforms, NumPasses,  Radices
            {     1024,           128,             1,         4,    { 8, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 } },
            //{      128,            64,             1,         7,   { 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
            {      128,            64,             4,         3,    { 8, 8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
            {        8,            64,            16,         3,    { 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },

          };
          size_t tableLength = sizeof(specRecord) / sizeof(specRecord[0]);

          for(size_t i = 0; i < tableLength; i++) {
            specTable[specRecord[i].length] = specRecord[i];
          }
        }
        break;

      default:
        assert(false);
    }
  }

  void GetRadices(size_t length, size_t &numPasses, const size_t* &pRadices) const {
    pRadices = NULL;
    numPasses = 0;
    typename SpecTable::const_iterator it = specTable.find(length);

    if(it != specTable.end()) {
      pRadices = it->second.radices;
      numPasses = it->second.numPasses;
    }
  }

  void GetWGSAndNT(size_t length, size_t &workGroupSize, size_t &numTransforms) const {
    workGroupSize = 0;
    numTransforms = 0;
    typename SpecTable::const_iterator it = specTable.find(length);

    if(it != specTable.end()) {
      workGroupSize = it->second.workGroupSize;
      numTransforms = it->second.numTransforms;
    }
  }
};

// Given the length of 1d fft, this function determines the appropriate work group size
// and the number of transforms per work group
// TODO for optimizations - experiment with different possibilities for work group sizes and num transforms for improving performance
void DetermineSizes(const size_t &MAX_WGS, const size_t &length, size_t &workGroupSize, size_t &numTrans, Precision &pr) {
  assert(MAX_WGS >= 64);

  if(length == 1) { // special case
    workGroupSize = 64;
    numTrans = 64;
    return;
  }

  size_t baseRadix[] = {13, 11, 7, 5, 3, 2}; // list only supported primes
  size_t baseRadixSize = sizeof(baseRadix) / sizeof(baseRadix[0]);
  size_t l = length;
  std::map<size_t, size_t> primeFactorsExpanded;

  for(size_t r = 0; r < baseRadixSize; r++) {
    size_t rad = baseRadix[r];
    size_t e = 1;

    while(!(l % rad)) {
      l /= rad;
      e *= rad;
    }

    primeFactorsExpanded[rad] = e;
  }

  assert(l == 1); // Makes sure the number is composed of only supported primes

  if    (primeFactorsExpanded[2] == length) { // Length is pure power of 2
    if    (length >= 1024)  {
      workGroupSize = (MAX_WGS >= 256) ? 256 : MAX_WGS;
      numTrans = 1;
    } else if (length == 512)   {
      workGroupSize = 64;
      numTrans = 1;
    } else if (length >= 16)    {
      workGroupSize = 64;
      numTrans = 256 / length;
    } else            {
      workGroupSize = 64;
      numTrans = 128 / length;
    }
  } else if (primeFactorsExpanded[3] == length) { // Length is pure power of 3
    workGroupSize = (MAX_WGS >= 256) ? 243 : 27;
    numTrans = length >= 3 * workGroupSize ? 1 : (3 * workGroupSize) / length;
  } else if (primeFactorsExpanded[5] == length) { // Length is pure power of 5
    workGroupSize = (MAX_WGS >= 128) ? 125 : 25;
    numTrans = length >= 5 * workGroupSize ? 1 : (5 * workGroupSize) / length;
  } else if (primeFactorsExpanded[7] == length) { // Length is pure power of 7
    workGroupSize = 49;
    numTrans = length >= 7 * workGroupSize ? 1 : (7 * workGroupSize) / length;
  } else if (primeFactorsExpanded[11] == length) { // Length is pure power of 11
		workGroupSize = 121;
		numTrans = length >= 11 * workGroupSize ? 1 : (11 * workGroupSize) / length;
	}	else if (primeFactorsExpanded[13] == length) { // Length is pure power of 13
		workGroupSize = 169;
		numTrans = length >= 13 * workGroupSize ? 1 : (13 * workGroupSize) / length;
	} else {
    size_t leastNumPerWI = 1; // least number of elements in one work item
    size_t maxWorkGroupSize = MAX_WGS; // maximum work group size desired

    if        (primeFactorsExpanded[2] * primeFactorsExpanded[3] == length) {
      if (length % 12 == 0) {
        leastNumPerWI = 12;
        maxWorkGroupSize = 128;
      } else {
        leastNumPerWI =  6;
        maxWorkGroupSize = 256;
      }
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[5] == length) {
      if (length % 20 == 0) {
        leastNumPerWI = 20;
        maxWorkGroupSize = 64;
      } else {
        leastNumPerWI = 10;
        maxWorkGroupSize = 128;
      }
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 14;
      maxWorkGroupSize = 64;
    } else if (primeFactorsExpanded[3] * primeFactorsExpanded[5] == length) {
      leastNumPerWI = 15;
      maxWorkGroupSize = 128;
    } else if (primeFactorsExpanded[3] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 21;
      maxWorkGroupSize = 128;
    } else if (primeFactorsExpanded[5] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 35;
      maxWorkGroupSize = 64;
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[3] * primeFactorsExpanded[5] == length) {
      leastNumPerWI = 30;
      maxWorkGroupSize = 64;
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[3] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 42;
      maxWorkGroupSize = 60;
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[5] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 70;
      maxWorkGroupSize = 36;
    } else if (primeFactorsExpanded[3] * primeFactorsExpanded[5] * primeFactorsExpanded[7] == length) {
      leastNumPerWI = 105;
      maxWorkGroupSize = 24;
    } else if (primeFactorsExpanded[2] * primeFactorsExpanded[11] == length) {
			leastNumPerWI = 22; maxWorkGroupSize = 128;
		}	else if (primeFactorsExpanded[2] * primeFactorsExpanded[13] == length) {
			leastNumPerWI = 26; maxWorkGroupSize = 128;
		} else {
      leastNumPerWI = 210;
      maxWorkGroupSize = 12;
    }
		if (pr==P_DOUBLE)
		{
			//leastNumPerWI /= 2; 
			maxWorkGroupSize /= 2;
		}

    if (maxWorkGroupSize > MAX_WGS) {
      maxWorkGroupSize = MAX_WGS;
    }

    assert (leastNumPerWI > 0 && length % leastNumPerWI == 0);

    for (size_t lnpi = leastNumPerWI; lnpi <= length; lnpi += leastNumPerWI) {
      if (length % lnpi != 0) {
        continue;
      }

      if (length / lnpi <= MAX_WGS) {
        leastNumPerWI = lnpi;
        break;
      }
    }

    numTrans = maxWorkGroupSize / (length / leastNumPerWI);
    numTrans = numTrans < 1 ? 1 : numTrans;
    workGroupSize = numTrans * (length / leastNumPerWI);
  }

  assert(workGroupSize <= MAX_WGS);
}

// Twiddle factors table
template <class T>
class TwiddleTable {
  size_t N; // length
  T* wc; // cosine, sine arrays

 public:
  TwiddleTable(size_t length) : N(length) {
    // Allocate memory for the tables
    // We compute twiddle factors in double precision for both P_SINGLE and P_DOUBLE
    wc = new T[N];
  }

  ~TwiddleTable() {
    // Free
    delete[] wc;
  }

  void GenerateTwiddleTable(void **twiddles, accelerator acc, const std::vector<size_t> &radices) {
    const double TWO_PI = -6.283185307179586476925286766559;
    // Make sure the radices vector sums up to N
    size_t sz = 1;

    for(std::vector<size_t>::const_iterator i = radices.begin();
        i != radices.end(); i++) {
      sz *= (*i);
    }

    assert(sz == N);
    // Generate the table
    size_t L = 1;
    size_t nt = 0;

    for(std::vector<size_t>::const_iterator i = radices.begin();
        i != radices.end(); i++) {
      size_t radix = *i;
      L *= radix;

      // Twiddle factors
      for(size_t k = 0; k < (L / radix); k++) {
        double theta = TWO_PI * ((double)k) / ((double)L);

        for(size_t j = 1; j < radix; j++) {
          double c = cos(((double)j) * theta);
          double s = sin(((double)j) * theta);
          //if (fabs(c) < 1.0E-12)  c = 0.0;
          //if (fabs(s) < 1.0E-12)  s = 0.0;
          wc[nt].x   = c;
          wc[nt++].y = s;
        }
      }
    }
    *twiddles = (T*)hc::am_alloc( N * sizeof(T), acc, 0);
    hc::am_copy(*twiddles, wc, N * sizeof(T));
    assert(twiddles != NULL);
  }
};

// A pass inside an FFT kernel
template <Precision PR>
class Pass {
  size_t position;          // Position in the kernel

  size_t algL;            // 'L' value from fft algorithm
  size_t algLS;           // 'LS' value
  size_t algR;            // 'R' value

  size_t length;            // Length of FFT
  size_t radix;           // Base radix
  size_t cnPerWI;           // Complex numbers per work-item

  size_t workGroupSize;       // size of the workgroup = (length / cnPerWI)
  // this number is essentially number of work-items needed to compute 1 transform
  // this number will be different from the kernel class workGroupSize if there
  // are multiple transforms per workgroup

  size_t numButterfly;        // Number of basic FFT butterflies = (cnPerWI / radix)
  size_t numB1, numB2, numB4;     // number of different types of butterflies

  bool r2c;             // real to complex transform
  bool c2r;             // complex to real transform
  bool rcFull;
  bool rcSimple;

  bool realSpecial;
  bool halfLds;
  bool enableGrouping;
  bool linearRegs;
  Pass<PR>* nextPass;

  inline void RegBase(size_t regC, std::string &str) const {
    str += "B";
    str += SztToStr(regC);
  }

  inline void RegBaseAndCount(size_t num, std::string &str) const {
    str += "C";
    str += SztToStr(num);
  }

  inline void RegBaseAndCountAndPos(const std::string &RealImag, size_t radPos, std::string &str) const {
    str += RealImag;
    str += SztToStr(radPos);
  }

  void RegIndex(size_t regC, size_t num, const std::string &RealImag, size_t radPos, std::string &str) const {
    RegBase(regC, str);
    RegBaseAndCount(num, str);
    RegBaseAndCountAndPos(RealImag, radPos, str);
  }

  void DeclareRegs(const std::string &regType, size_t regC, size_t numB, std::string &passStr) const {
    std::string regBase;
    RegBase(regC, regBase);

    if(linearRegs) {
      assert(regC == 1);
      assert(numB == numButterfly);
    }

    for(size_t i = 0; i < numB; i++) {
      passStr += "\n\t";
      passStr += regType;
      passStr += " ";
      std::string regBaseCount = regBase;
      RegBaseAndCount(i, regBaseCount);

      for(size_t r = 0; ; r++) {
        if(linearRegs) {
          std::string regIndex = "R";
          RegBaseAndCountAndPos("", i * radix + r, regIndex);
          passStr += regIndex;
        } else {
          std::string regRealIndex(regBaseCount), regImagIndex(regBaseCount);
          RegBaseAndCountAndPos("R", r, regRealIndex); // real
          RegBaseAndCountAndPos("I", r, regImagIndex); // imaginary
          passStr += regRealIndex;
          passStr += ", ";
          passStr += regImagIndex;
        }

        if(r == radix - 1) {
          passStr += ";";
          break;
        } else {
          passStr += ", ";
        }
      }
    }
  }

  inline std::string IterRegArgs() const {
    std::string str = "";

    if(linearRegs) {
      std::string regType = RegBaseType<PR>(2);

      for(size_t i = 0; i < cnPerWI; i++) {
        if(i != 0) {
          str += ", ";
        }

        str += regType;
        str += " *R";
        str += SztToStr(i);
      }
    }

    return str;
  }

#define SR_READ     1
#define SR_TWMUL    2
#define SR_TWMUL_3STEP  3
#define SR_WRITE    4

#define SR_COMP_REAL 0 // real
#define SR_COMP_IMAG 1 // imag
#define SR_COMP_BOTH 2 // real & imag

  // SweepRegs is to iterate through the registers to do the three basic operations:
  // reading, twiddle multiplication, writing
  void SweepRegs( const hcfftPlanHandle plHandle, size_t flag, bool fwd, bool interleaved, size_t stride, size_t component,
                  double scale, bool frontTwiddle,
                  const std::string &bufferRe, const std::string &bufferIm, const std::string &offset,
                  size_t regC, size_t numB, size_t numPrev, std::string &passStr, bool isPrecallVector = false, bool oddt = false) const {
    assert( (flag == SR_READ )      ||
            (flag == SR_TWMUL)      ||
            (flag == SR_TWMUL_3STEP)  ||
            (flag == SR_WRITE) );
    const std::string twTable = TwTableName();
    const std::string tw3StepFunc = TwTableLargeFunc();
    // component: 0 - real, 1 - imaginary, 2 - both
    size_t cStart, cEnd;

    switch(component) {
      case SR_COMP_REAL:
        cStart = 0;
        cEnd = 1;
        break;

      case SR_COMP_IMAG:
        cStart = 1;
        cEnd = 2;
        break;

      case SR_COMP_BOTH:
        cStart = 0;
        cEnd = 2;
        break;

      default:
        assert(false);
    }

    // Read/Write logic:
    // The double loop inside pass loop of FFT algorithm is mapped into the
    // workGroupSize work items with each work item handling cnPerWI numbers
    // Read logic:
    // Reads for any pass appear the same with the stockham algorithm when mapped to
    // the work items. The buffer is divided into (L/radix) sized blocks and the
    // values are read in linear order inside each block.
    // Vector reads are possible if we have unit strides
    // since read pattern remains the same for all passes and they are contiguous
    // Writes are not contiguous
    // TODO : twiddle multiplies can be combined with read
    // TODO : twiddle factors can be reordered in the table to do vector reads of them
    // Write logic:
    // outer loop index k and the inner loop index j map to 'me' as follows:
    // In one work-item (1 'me'), there are 'numButterfly' fft butterflies. They
    // are indexed as numButterfly*me + butterflyIndex, where butterflyIndex's range is
    // 0 ... numButterfly-1. The total number of butterflies needed is covered over all
    // the work-items. So essentially the double loop k,j is flattened to fit this linearly
    // increasing 'me'.
    // j = (numButterfly*me + butterflyIndex)%LS
    // k = (numButterfly*me + butterflyIndex)/LS
    std::string twType = RegBaseType<PR>(2);
    std::string rType  = RegBaseType<PR>(1);
    size_t butterflyIndex = numPrev;
  	std::string bufOffset;

    std::string regBase;
    RegBase(regC, regBase);

    // special write back to global memory with float4 grouping, writing 2 complex numbers at once
    if( numB && (numB % 2 == 0) && (regC == 1) && (stride == 1) && (numButterfly % 2 == 0) && (algLS % 2 == 0) && (flag == SR_WRITE) &&
        (nextPass == NULL) && interleaved && (component == SR_COMP_BOTH) && linearRegs && enableGrouping ) {
      assert((numButterfly * workGroupSize) == algLS);
      assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
      passStr += "\n\t";
      passStr += RegBaseType<PR>(4);
      passStr += " *buff4g = (";
      passStr += RegBaseType<PR>(4);
      passStr += "*)";
      passStr += bufferRe;
      passStr += ";\n\t";

      for(size_t r = 0; r < radix; r++) { // setting the radix loop outside to facilitate grouped writing
        butterflyIndex = numPrev;

        for(size_t i = 0; i < (numB / 2); i++) {
          std::string regIndexA = "R";
          std::string regIndexB = "R";
          RegBaseAndCountAndPos("", (2 * i + 0)*radix + r, regIndexA);
          regIndexA += "[0]";
          RegBaseAndCountAndPos("", (2 * i + 1)*radix + r, regIndexB);
          regIndexB += "[0]";
          passStr += "\n\t";
          passStr += "buff4g";
          passStr += "[ ";
          passStr += SztToStr(numButterfly / 2);
          passStr += "*me + ";
          passStr += SztToStr(butterflyIndex);
          passStr += " + ";
          passStr += SztToStr(r * (algLS / 2));
          passStr += " ]";
          passStr += " = ";
          passStr += RegBaseType<PR>(4);
          passStr += "(";
          passStr += regIndexA;
          passStr += ".x, ";
          passStr += regIndexA;
          passStr += ".y, ";
          passStr += regIndexB;
          passStr += ".x, ";
          passStr += regIndexB;
          passStr += ".y) ";

          if(scale != 1.0f) {
            passStr += " * ";
            passStr += FloatToStr(scale);
            passStr += FloatSuffix<PR>();
          }

          passStr += ";";
          butterflyIndex++;
        }
      }

      return;
    }

    size_t hid = 0;
    bool swapElement = false;
    size_t tIter = numB * radix;

    // block to rearrange reads of adjacent memory locations together
    if(linearRegs && (flag == SR_READ)) {
      for(size_t r = 0; r < radix; r++) {
        for(size_t i = 0; i < numB; i++) {
          for(size_t c = cStart; c < cEnd; c++) { // component loop: 0 - real, 1 - imaginary
            std::string tail;
            std::string regIndex;
            std::string regIndexC;
            regIndex = "(R";
            std::string buffer;

            // Read real & imag at once
            if(interleaved && (component == SR_COMP_BOTH)) {
              assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
              buffer = bufferRe;
              RegBaseAndCountAndPos("", i * radix + r, regIndex);
              regIndex += "[0])";
              tail = ";";
            } else {
              if(c == 0) {
                RegBaseAndCountAndPos("", i * radix + r, regIndex);
		hid = (i * radix + r) / ( tIter > 1 ? (tIter / 2) : 1 );
		swapElement = swapElement && hid != 0;
		swapElement = (oddt && ((i * radix + r) >= (tIter - 1))) ? false : swapElement;  //for c2r odd size don't swap for last register
		if (swapElement)
		{
		regIndexC = regIndex; regIndexC += "[0]).y";
		}
                regIndex += "[0]).x";
                buffer = bufferRe;
                tail = interleaved ? ".x;" : ";";
              } else {
                RegBaseAndCountAndPos("", i * radix + r, regIndex);
                regIndex += "[0]).y";
                buffer = bufferIm;
                tail = interleaved ? ".y;" : ";";
              }
            }

	//get offset 
	bufOffset.clear();
	bufOffset += offset; bufOffset += " + ( "; bufOffset += SztToStr(numPrev); bufOffset += " + ";
	bufOffset += "me*"; bufOffset += SztToStr(numButterfly); bufOffset += " + ";
	bufOffset += SztToStr(i); bufOffset += " + ";
	bufOffset += SztToStr(r*length/radix); bufOffset += " )*";
	bufOffset += SztToStr(stride);

	if (swapElement)
        {
	passStr += "\n\t";
	passStr += regIndexC; passStr += " = "; passStr += regIndex; passStr += ";";
	}

	passStr += "\n\t";
	passStr += regIndex;
	passStr += " = ";

	passStr += buffer;
	passStr += "["; passStr += bufOffset; passStr += "]"; passStr += tail;

        // Since we read real & imag at once, we break the loop
        if(interleaved && (component == SR_COMP_BOTH) ) {
           break;
        }
        }
        }
      }

      return;
    }

    // block to rearrange writes of adjacent memory locations together
    if(linearRegs && (flag == SR_WRITE) && (nextPass == NULL)) {
      for(size_t r = 0; r < radix; r++) {
        butterflyIndex = numPrev;

        for(size_t i = 0; i < numB; i++) {
          if(realSpecial && (nextPass == NULL) && (r > (radix / 2))) {
            break;
          }

          if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i != 0)) {
            break;
          }

          if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i == 0)) {
            passStr += "\n\t}\n\tif( rw && !me)\n\t{";
          }

					std::string regIndexC0;
          for(size_t c = cStart; c < cEnd; c++) { // component loop: 0 - real, 1 - imaginary
            std::string tail;
            std::string regIndex;
            regIndex = "(R";
            std::string buffer;

            // Write real & imag at once
            if(interleaved && (component == SR_COMP_BOTH)) {
              assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
              buffer = bufferRe;
              RegBaseAndCountAndPos("", i * radix + r, regIndex);
              regIndex += "[0])";
              tail = "";
            } else {
              if(c == 0) {
                RegBaseAndCountAndPos("", i * radix + r, regIndex);
                regIndex += "[0]).x";
                buffer = bufferRe;
                tail = interleaved ? ".x" : "";
              } else {
                RegBaseAndCountAndPos("", i * radix + r, regIndex);
                regIndex += "[0]).y";
                buffer = bufferIm;
                tail = interleaved ? ".y" : "";
              }
            }

	    bufOffset.clear();
	    bufOffset += offset; bufOffset += " + ( "; 

            if( (numButterfly * workGroupSize) > algLS ) {
              bufOffset += "((";
              bufOffset += SztToStr(numButterfly);
              bufOffset += "*me + ";
              bufOffset += SztToStr(butterflyIndex);
              bufOffset += ")/";
              bufOffset += SztToStr(algLS);
              bufOffset += ")*";
              passStr += SztToStr(algL);
              bufOffset += " + (";
              bufOffset += SztToStr(numButterfly);
              bufOffset += "*me + ";
              bufOffset += SztToStr(butterflyIndex);
              bufOffset += ")%";
              bufOffset += SztToStr(algLS);
              bufOffset += " + ";
            } else {
              bufOffset += SztToStr(numButterfly);
              bufOffset += "*me + ";
              bufOffset += SztToStr(butterflyIndex);
              bufOffset += " + ";
            }

            bufOffset += SztToStr(r * algLS);
            bufOffset += " )*";
            bufOffset += SztToStr(stride);

	    if(scale != 1.0f) { regIndex += " * "; regIndex += FloatToStr(scale); regIndex += FloatSuffix<PR>(); }
	    if (c == cStart)	regIndexC0 = regIndex;

	    passStr += "\n\t";
	    passStr += buffer; passStr += "["; passStr += bufOffset; passStr += "]";
	    passStr += tail; passStr += " = "; passStr += regIndex; passStr += ";";

            // Since we write real & imag at once, we break the loop
            if(interleaved && (component == SR_COMP_BOTH)) {
              break;
            }
          }

          if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i == 0)) {
            passStr += "\n\t}\n\tif(rw)\n\t{";
          }

          butterflyIndex++;
        }
      }

      return;
    }

    for(size_t i = 0; i < numB; i++) {
      std::string regBaseCount = regBase;
      RegBaseAndCount(i, regBaseCount);

      if(flag == SR_READ) { // read operation
        // the 'r' (radix index) loop is placed outer to the
        // 'v' (vector index) loop to make possible vectorized reads
        for(size_t r = 0; r < radix; r++) {
          for(size_t c = cStart; c < cEnd; c++) { // component loop: 0 - real, 1 - imaginary
            std::string tail;
            std::string regIndex;
						std::string regIndexC;
            regIndex = linearRegs ? "(R" : regBaseCount;
            std::string buffer;

            // Read real & imag at once
            if(interleaved && (component == SR_COMP_BOTH) && linearRegs) {
              assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
              buffer = bufferRe;
              RegBaseAndCountAndPos("", i * radix + r, regIndex);
              regIndex += "[0])";
              tail = ";";
            } else {
              if(c == 0) {
                if(linearRegs) {
                  RegBaseAndCountAndPos("", i * radix + r, regIndex);
									hid = (i * radix + r) / (numB * radix / 2);
                  regIndex += "[0]).x";
                } else       {
                  RegBaseAndCountAndPos("R", r, regIndex);
                }

                buffer = bufferRe;
                tail = interleaved ? ".x;" : ";";
              } else {
                if(linearRegs) {
                  RegBaseAndCountAndPos("", i * radix + r, regIndex);
                  regIndex += "[0]).y";
                } else       {
                  RegBaseAndCountAndPos("I", r, regIndex);
                }

                buffer = bufferIm;
                tail = interleaved ? ".y;" : ";";
              }
            }

            for(size_t v = 0; v < regC; v++) { // TODO: vectorize the reads; instead of reading individually for consecutive reads of vector elements
              std::string regIndexSub(regIndex);

              if(regC != 1) {
                regIndexSub += ((v == 0) ? ".x" : (v == 1) ? ".y " : (v == 2) ? ".z" : ".w");
              }

	     //get offset 
	     bufOffset.clear();
	     bufOffset += offset; bufOffset += " + ( "; bufOffset += SztToStr(numPrev); bufOffset += " + ";
	     bufOffset += "me*"; bufOffset += SztToStr(numButterfly); bufOffset += " + ";
	     bufOffset += SztToStr(i*regC + v); bufOffset += " + ";
	     bufOffset += SztToStr(r*length/radix); bufOffset += " )*";
	     bufOffset += SztToStr(stride);

	     passStr += "\n\t";
	     passStr += regIndexSub;
	     passStr += " = "; 

	     passStr += buffer;
	     passStr += "["; passStr += bufOffset; passStr += "]"; passStr += tail;

            }

            // Since we read real & imag at once, we break the loop
            if(interleaved && (component == SR_COMP_BOTH) && linearRegs) {
              break;
            }
          }
        }
      } else if( (flag == SR_TWMUL) || (flag == SR_TWMUL_3STEP) ) { // twiddle multiplies and writes require that 'r' loop be innermost
        for(size_t v = 0; v < regC; v++) {
          for(size_t r = 0; r < radix; r++) {
            std::string regRealIndex, regImagIndex;
            regRealIndex = linearRegs ? "(R" : regBaseCount;
            regImagIndex = linearRegs ? "(R" : regBaseCount;

            if(linearRegs) {
              RegBaseAndCountAndPos("", i * radix + r, regRealIndex);
              regRealIndex += "[0]).x";
              RegBaseAndCountAndPos("", i * radix + r, regImagIndex);
              regImagIndex += "[0]).y";
            } else {
              RegBaseAndCountAndPos("R", r, regRealIndex);
              RegBaseAndCountAndPos("I", r, regImagIndex);
            }

            if(regC != 1) {
              regRealIndex += ((v == 0) ? ".x" : (v == 1) ? ".y " : (v == 2) ? ".z" : ".w");
              regImagIndex += ((v == 0) ? ".x" : (v == 1) ? ".y " : (v == 2) ? ".z" : ".w");
            }

            if(flag == SR_TWMUL) { // twiddle multiply operation
              if(r == 0) { // no twiddle muls needed
                continue;
              }

              passStr += "\n\t{\n\t\t";
              passStr += twType;
              passStr += " W = ";
              passStr += twTable;
              passStr += "[";
              passStr += SztToStr(algLS - 1);
              passStr += " + ";
              passStr += SztToStr(radix - 1);
              passStr += "*((";
              passStr += SztToStr(numButterfly);
              passStr += "*me + ";
              passStr += SztToStr(butterflyIndex);
              passStr += ")%";
              passStr += SztToStr(algLS);
              passStr += ") + ";
              passStr += SztToStr(r - 1);
              passStr += "];\n\t\t";
            } else { // 3-step twiddle
              passStr += "\n\t{\n\t\t";
              passStr += twType;
              passStr += " W = ";
              passStr += tw3StepFunc;
              passStr += SztToStr(plHandle);
              passStr += "( ";

              if(frontTwiddle) {
                assert(linearRegs);
                passStr += "(";
                passStr += "me*";
                passStr += SztToStr(numButterfly);
                passStr += " + ";
                passStr += SztToStr(i);
                passStr += " + ";
                passStr += SztToStr(r * length / radix);
                passStr += ") * b";
              } else {
                passStr += "((";
                passStr += SztToStr(numButterfly);
                passStr += "*me + ";
                passStr += SztToStr(butterflyIndex);
                passStr += ")%";
                passStr += SztToStr(algLS);
                passStr += " + ";
                passStr += SztToStr(r * algLS);
                passStr += ") * b";
              }

              passStr += ",";
              passStr += TwTableLargeName();
              passStr += ");\n\t\t";
            }

            passStr += rType;
            passStr += " TR, TI;\n\t\t";

            if(realSpecial && (flag == SR_TWMUL_3STEP)) {
              if(fwd) {
                passStr += "if(t==0)\n\t\t{\n\t\t";
                passStr += "TR = (W.x * ";
                passStr += regRealIndex;
                passStr += ") - (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = (W.y * ";
                passStr += regRealIndex;
                passStr += ") + (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "}\n\t\telse\n\t\t{\n\t\t";
                passStr += "TR = (W.x * ";
                passStr += regRealIndex;
                passStr += ") + (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = (W.y * ";
                passStr += regRealIndex;
                passStr += ") - (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "}\n\t\t";
              } else {
                passStr += "if(t==0)\n\t\t{\n\t\t";
                passStr += "TR = (W.x * ";
                passStr += regRealIndex;
                passStr += ") + (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = (W.y * ";
                passStr += regRealIndex;
                passStr += ") - (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "}\n\t\telse\n\t\t{\n\t\t";
                passStr += "TR = (W.x * ";
                passStr += regRealIndex;
                passStr += ") - (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = (W.y * ";
                passStr += regRealIndex;
                passStr += ") + (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "}\n\t\t";
              }
            } else {
              if(fwd) {
                passStr += "TR = (W.x * ";
                passStr += regRealIndex;
                passStr += ") - (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = (W.y * ";
                passStr += regRealIndex;
                passStr += ") + (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
              } else {
                passStr += "TR =  (W.x * ";
                passStr += regRealIndex;
                passStr += ") + (W.y * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
                passStr += "TI = -(W.y * ";
                passStr += regRealIndex;
                passStr += ") + (W.x * ";
                passStr += regImagIndex;
                passStr += ");\n\t\t";
              }
            }

            passStr += regRealIndex;
            passStr += " = TR;\n\t\t";
            passStr += regImagIndex;
            passStr += " = TI;\n\t}\n";
          }

          butterflyIndex++;
        }
      } else { // write operation
        for(size_t v = 0; v < regC; v++) {
          for(size_t r = 0; r < radix; r++) {
            if(realSpecial && (nextPass == NULL) && (r > (radix / 2))) {
              break;
            }

            if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i != 0)) {
              break;
            }

            if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i == 0)) {
              passStr += "\n\t}\n\tif( rw && !me)\n\t{";
            }

						std::string regIndexC0;

            for(size_t c = cStart; c < cEnd; c++) { // component loop: 0 - real, 1 - imaginary
              std::string tail;
              std::string regIndex;
              regIndex = linearRegs ? "(R" : regBaseCount;
              std::string buffer;

              // Write real & imag at once
              if(interleaved && (component == SR_COMP_BOTH) && linearRegs) {
                assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
                buffer = bufferRe;
                RegBaseAndCountAndPos("", i * radix + r, regIndex);
                regIndex += "[0])";
                tail = "";
              } else {
                if(c == 0) {
                  if(linearRegs) {
                    RegBaseAndCountAndPos("", i * radix + r, regIndex);
                    regIndex += "[0]).x";
                  } else       {
                    RegBaseAndCountAndPos("R", r, regIndex);
                  }

                  buffer = bufferRe;
                  tail = interleaved ? ".x" : "";
                } else {
                  if(linearRegs) {
                    RegBaseAndCountAndPos("", i * radix + r, regIndex);
                    regIndex += "[0]).y";
                  } else       {
                    RegBaseAndCountAndPos("I", r, regIndex);
                  }

                  buffer = bufferIm;
                  tail = interleaved ? ".y" : "";
                }
              }

              if(regC != 1) {
                regIndex += ((v == 0) ? ".x" : (v == 1) ? ".y " : (v == 2) ? ".z" : ".w");
              }

              passStr += "\n\t";

	      if(scale != 1.0f) { regIndex += " * "; regIndex += FloatToStr(scale); regIndex += FloatSuffix<PR>(); }
	      if (c == 0) regIndexC0 += regIndex;

	      bufOffset.clear();
	      bufOffset += offset; bufOffset += " + ( ";

              if( (numButterfly * workGroupSize) > algLS ) {
                bufOffset += "((";
                bufOffset += SztToStr(numButterfly);
                bufOffset += "*me + ";
                bufOffset += SztToStr(butterflyIndex);
                bufOffset += ")/";
                bufOffset += SztToStr(algLS);
                bufOffset += ")*";
                bufOffset += SztToStr(algL);
                bufOffset += " + (";
                bufOffset += SztToStr(numButterfly);
                bufOffset += "*me + ";
                bufOffset += SztToStr(butterflyIndex);
                bufOffset += ")%";
                bufOffset += SztToStr(algLS);
                bufOffset += " + ";
              } else {
                bufOffset += SztToStr(numButterfly);
                bufOffset += "*me + ";
                bufOffset += SztToStr(butterflyIndex);
                bufOffset += " + ";
              }

              bufOffset += SztToStr(r * algLS);
              bufOffset += " )*";
              bufOffset += SztToStr(stride);


	      passStr += buffer; passStr += "["; passStr += bufOffset; passStr += "]";
	      passStr += tail; passStr += " = "; passStr += regIndex;
	      passStr += ";";

              // Since we write real & imag at once, we break the loop
              if(interleaved && (component == SR_COMP_BOTH) && linearRegs) {
                break;
              }
            }

            if(realSpecial && (nextPass == NULL) && (r == radix / 2) && (i == 0)) {
              passStr += "\n\t}\n\tif(rw)\n\t{";
            }
          }

          butterflyIndex++;
        }
      }
    }

    assert(butterflyIndex <= numButterfly);
  }

  // Special SweepRegs function to carry out some R-C/C-R specific operations
  void SweepRegsRC( size_t flag, bool fwd, bool interleaved, size_t stride, size_t component,
                    double scale, bool setZero, bool batch2, bool oddt,
                    const std::string &bufferRe, const std::string &bufferIm, const std::string &offset,
                    std::string &passStr) const {
    assert( (flag == SR_READ ) ||
            (flag == SR_WRITE) );
    // component: 0 - real, 1 - imaginary, 2 - both
    size_t cStart, cEnd;

    switch(component) {
      case SR_COMP_REAL:
        cStart = 0;
        cEnd = 1;
        break;

      case SR_COMP_IMAG:
        cStart = 1;
        cEnd = 2;
        break;

      case SR_COMP_BOTH:
        cStart = 0;
        cEnd = 2;
        break;

      default:
        assert(false);
    }

    std::string rType  = RegBaseType<PR>(1);
    assert(r2c || c2r);
    assert(linearRegs);
    bool singlePass = ((position == 0) && (nextPass == NULL));
    size_t numCR = numButterfly * radix;

    if(!(numCR % 2)) {
      assert(!oddt);
    }

    size_t rStart = 0;
    size_t rEnd = numCR;
    bool oddp = ((numCR % 2) && (numCR > 1) && !setZero);

    if(oddp) {
      if(oddt)  {
        rStart = numCR - 1;
        rEnd = numCR + 1;
      } else    {
        rStart = 0;
        rEnd = numCR - 1;
      }
    }

    if(!oddp) {
      assert(!oddt);
    }

    for(size_t r = rStart; r < rEnd; r++) {
			std::string val1StrExt;
      for(size_t c = cStart; c < cEnd; c++) { // component loop: 0 - real, 1 - imaginary
        if(flag == SR_READ) { // read operation
          std::string tail, tail2;
          std::string regIndex = "(R";
          std::string buffer;
					RegBaseAndCountAndPos("", r, regIndex); 
          if(c == 0) {
            regIndex += "[0]).x";
            buffer = bufferRe;
            tail  = interleaved ? ".x;" : ";";
            tail2 = interleaved ? ".y;" : ";";
          } else {
            regIndex += "[0]).y";
            buffer = bufferIm;
            tail  = interleaved ? ".y;" : ";";
            tail2 = interleaved ? ".x;" : ";";
          }

          size_t bid = numCR / 2;
          bid = bid ? bid : 1;
          size_t cid, lid;

          if(oddt) {
            cid = r % 2;
            lid = 1 + (numCR / 2);
          } else {
            cid = r / bid;
            lid = 1 + r % bid;
          }

          std::string oddpadd = oddp ? " (me/2) + " : " ";
          std::string idxStr, idxStrRev;

          if((length <= 2) || ((length & (length - 1)) != 0)) {
            idxStr += SztToStr(bid);
            idxStr += "*me +";
            idxStr += oddpadd;
            idxStr += SztToStr(lid);
          } else {
            idxStr += "me + ";
            idxStr += SztToStr(1 + length * (r % bid) / numCR);
            idxStr += oddpadd;
          }

          idxStrRev += SztToStr(length);
          idxStrRev += " - (";
          idxStrRev += idxStr;
          idxStrRev += " )";
          bool act = ( fwd || ((cid == 0) && (!batch2)) || ((cid != 0) && batch2) );

          if(act) {
            passStr += "\n\t";
            passStr += regIndex;
            passStr += " = ";
          }

          if(setZero) {
            if(act) {
              passStr += "0;";
            }
          } else {
            if(act) {
              passStr += buffer;
              passStr += "[";
              passStr += offset;
              passStr += " + ( ";
            }

            if(fwd) {
              if(cid == 0) {
                passStr += idxStr;
              } else {
                passStr += idxStrRev;
              }
            } else {
              if(cid == 0)  {
                if(!batch2) {
                  passStr += idxStr;
                }
              } else      {
                if(batch2) {
                  passStr += idxStr;
                }
              }
            }

            if(act) {
              passStr += " )*";
              passStr += SztToStr(stride);
              passStr += "]";

              if(fwd) {
                passStr += tail;
              } else  {
                if(!batch2) {
                  passStr += tail;
                } else {
                  passStr += tail2;
                }
              }
            }
          }
        } else { // write operation
          std::string tail;
          std::string regIndex = "(R";
          std::string regIndexPair = "(R";
          std::string buffer;

          // Write real & imag at once
          if(interleaved && (component == SR_COMP_BOTH)) {
            assert(bufferRe.compare(bufferIm) == 0); // Make sure Real & Imag buffer strings are same for interleaved data
            buffer = bufferRe;
          } else {
            if(c == 0) {
              buffer = bufferRe;
              tail = interleaved ? ".x" : "";
            } else {
              buffer = bufferIm;
              tail = interleaved ? ".y" : "";
            }
          }

          size_t bid, cid, lid;

          if(singlePass && fwd) {
            bid = 1 + radix / 2;
            lid = r;
            cid = r / bid;
            RegBaseAndCountAndPos("", r, regIndex);
            regIndex += "[0])";
            RegBaseAndCountAndPos("", (radix - r) % radix , regIndexPair);
            regIndexPair += "[0])";
          } else {
            bid = numCR / 2;

            if(oddt) {
              cid = r % 2;
              lid = 1 + (numCR / 2);
              RegBaseAndCountAndPos("", r, regIndex);
              regIndex += "[0])";
              RegBaseAndCountAndPos("", r + 1, regIndexPair);
              regIndexPair += "[0])";
            } else {
              cid = r / bid;
              lid = 1 + r % bid;
              RegBaseAndCountAndPos("", r, regIndex);
              regIndex += "[0])";
              RegBaseAndCountAndPos("", r + bid, regIndexPair);
              regIndexPair += "[0])";
            }
          }

          if(!cid) {
            std::string oddpadd = oddp ? " (me/2) + " : " ";
            std::string sclStr = "";

            if(scale != 1.0f) {
              sclStr += " * ";
              sclStr += FloatToStr(scale);
              sclStr += FloatSuffix<PR>();
            }

            if(fwd) {
              std::string idxStr, idxStrRev;

              if((length <= 2) || ((length & (length - 1)) != 0)) {
                idxStr += SztToStr(length / (2 * workGroupSize));
                idxStr += "*me +";
                idxStr += oddpadd;
                idxStr += SztToStr(lid);
              } else {
                idxStr += "me + ";
                idxStr += SztToStr(1 + length * (r % bid) / numCR);
                idxStr += oddpadd;
              }

              idxStrRev += SztToStr(length);
              idxStrRev += " - (";
              idxStrRev += idxStr;
              idxStrRev += " )";
              std::string val1Str, val2Str;

              val1Str += "\n\t";
              val1Str += buffer;
              val1Str += "[";
              val1Str += offset;
              val1Str += " + ( ";
              val1Str += idxStr;
              val1Str += " )*";
              val1Str += SztToStr(stride);
              val1Str += "]";
              val1Str += tail;
              val1Str += " = ";

              val2Str += "\n\t";
              val2Str += buffer;
              val2Str += "[";
              val2Str += offset;
              val2Str += " + ( ";
              val2Str += idxStrRev;
              val2Str += " )*";
              val2Str += SztToStr(stride);
              val2Str += "]";
              val2Str += tail;
              val2Str += " = ";
              std::string real1, imag1, real2, imag2;
              real1 +=  "(";
              real1 += regIndex;
              real1 += ".x + ";
              real1 += regIndexPair;
              real1 += ".x)*0.5";
              imag1 +=  "(";
              imag1 += regIndex;
              imag1 += ".y - ";
              imag1 += regIndexPair;
              imag1 += ".y)*0.5";
              real2 +=  "(";
              real2 += regIndex;
              real2 += ".y + ";
              real2 += regIndexPair;
              real2 += ".y)*0.5";
              imag2 += "(-";
              imag2 += regIndex;
              imag2 += ".x + ";
              imag2 += regIndexPair;
              imag2 += ".x)*0.5";

              if(interleaved && (component == SR_COMP_BOTH)) {
                val1Str += RegBaseType<PR>(2);
                val1Str += "( ";
                val2Str += RegBaseType<PR>(2);
                val2Str += "( ";

                if(!batch2) {
                  val1Str += real1;
                  val1Str += ", ";
                  val1Str += "+";
                  val1Str += imag1;
                  val2Str += real1;
                  val2Str += ", ";
                  val2Str += "-";
                  val2Str += imag1;
                } else    {
                  val1Str += real2;
                  val1Str += ", ";
                  val1Str += "+";
                  val1Str += imag2;
                  val2Str += real2;
                  val2Str += ", ";
                  val2Str += "-";
                  val2Str += imag2;
                }

                val1Str += " )";
                val2Str += " )";
              } else {
                val1Str += " (";
                val2Str += " (";

                if(c == 0) {
                  if(!batch2) {
                    val1Str += real1;
                    val2Str += real1;
                  } else    {
                    val1Str += real2;
                    val2Str += real2;
                  }
                } else {
                  if(!batch2) {
                    val1Str += "+";
                    val1Str += imag1;
                    val2Str += "-";
                    val2Str += imag1;
                  } else    {
                    val1Str += "+";
                    val1Str += imag2;
                    val2Str += "-";
                    val2Str += imag2;
                  }
                }

                val1Str += " )";
                val2Str += " )";
              }

              val1Str += sclStr;
              val2Str += sclStr;

              val1Str += ";";

              passStr += val1Str;
              if(rcFull)  {
                passStr += val2Str;
                passStr += ";";
              }
            } else {
              std::string idxStr, idxStrRev;

              if((length <= 2) || ((length & (length - 1)) != 0)) {
                idxStr += SztToStr(bid);
                idxStr += "*me +";
                idxStr += oddpadd;
                idxStr += SztToStr(lid);
              } else {
                idxStr += "me + ";
                idxStr += SztToStr(1 + length * (r % bid) / numCR);
                idxStr += oddpadd;
              }

              idxStrRev += SztToStr(length);
              idxStrRev += " - (";
              idxStrRev += idxStr;
              idxStrRev += " )";
              passStr += "\n\t";
              passStr += buffer;
              passStr += "[";
              passStr += offset;
              passStr += " + ( ";

              if(!batch2) {
                passStr += idxStr;
              } else {
                passStr += idxStrRev;
              }

              passStr += " )*";
              passStr += SztToStr(stride);
              passStr += "]";
              passStr += tail;
              passStr += " = ";
              passStr += "( ";

              if(c == 0) {
                regIndex += ".x";
                regIndexPair += ".x";

                if(!batch2) {
                  passStr += regIndex;
                  passStr += " - ";
                  passStr += regIndexPair;
                } else    {
                  passStr += regIndex;
                  passStr += " + ";
                  passStr += regIndexPair;
                }
              } else {
                regIndex += ".y";
                regIndexPair += ".y";

                if(!batch2) {
                  passStr += regIndex;
                  passStr += " + ";
                  passStr += regIndexPair;
                } else    {
                  passStr += " - ";
                  passStr += regIndex;
                  passStr += " + ";
                  passStr += regIndexPair;
                }
              }

              passStr += " )";
              passStr += sclStr;
              passStr += ";";
            }

            // Since we write real & imag at once, we break the loop
            if(interleaved && (component == SR_COMP_BOTH)) {
              break;
            }
          }
        }
      }
    }
  }

  void CallButterfly(const std::string &bflyName, size_t regC, size_t numB, std::string &passStr) const {
    std::string regBase;
    RegBase(regC, regBase);

    for(size_t i = 0; i < numB; i++) {
      std::string regBaseCount = regBase;
      RegBaseAndCount(i, regBaseCount);
      passStr += "\n\t";
      passStr += bflyName;
      passStr += "(";

      for(size_t r = 0; ; r++) {
        if(linearRegs) {
          std::string regIndex = "R";
          RegBaseAndCountAndPos("", i * radix + r, regIndex);
          passStr += regIndex;
        } else {
          std::string regRealIndex(regBaseCount);
          std::string regImagIndex(regBaseCount);
          RegBaseAndCountAndPos("R", r, regRealIndex);
          RegBaseAndCountAndPos("I", r, regImagIndex);
          passStr += "&";
          passStr += regRealIndex;
          passStr += ", ";
          passStr += "&";
          passStr += regImagIndex;
        }

        if(r == radix - 1) {
          passStr += ");";
          break;
        } else {
          passStr += ", ";
        }
      }
    }
  }
 public:
  Pass( size_t positionVal, size_t lengthVal, size_t radixVal, size_t cnPerWIVal,
        size_t L, size_t LS, size_t R, bool linearRegsVal, bool halfLdsVal,
        bool r2cVal, bool c2rVal, bool rcFullVal, bool rcSimpleVal, bool realSpecialVal) :
    position(positionVal), algL(L), algLS(LS), algR(R), length(lengthVal), radix(radixVal),
    cnPerWI(cnPerWIVal), numB1(0), numB2(0), numB4(0),
    r2c(r2cVal), c2r(c2rVal), rcFull(rcFullVal), rcSimple(rcSimpleVal), realSpecial(realSpecialVal),
    halfLds(halfLdsVal), enableGrouping(true), linearRegs(linearRegsVal),
    nextPass(NULL) {
    assert(radix <= length);
    assert(length % radix == 0);
    numButterfly = cnPerWI / radix;
    workGroupSize = length / cnPerWI;
    // Total number of butterflies (over all work-tems) must be divisible by LS
    assert( ((numButterfly * workGroupSize) % algLS) == 0 );
    // All butterflies in one work-item should always be part of no more than 1 FFT transform.
    // In other words, there should not be more than 1 FFT transform per work-item.
    assert(cnPerWI <= length);

    // Calculate the different types of Butterflies needed
    if(linearRegs || r2c || c2r) {
      numB1 = numButterfly;
    } else {
      numB4 = numButterfly / 4;
      numB2 = (numButterfly % 4) / 2; // can be 0 or 1
      numB1 = (numButterfly % 2); // can be 0 or 1
      assert(numButterfly == (numB4 * 4 + numB2 * 2 + numB1));
    }

    // if only half LDS can be used, we need the passes to share registers
    // and hence they need to be linear registers
    if(halfLds) {
      assert(linearRegs);
    }
  }

  size_t GetNumB1() const {
    return numB1;
  }
  size_t GetNumB2() const {
    return numB2;
  }
  size_t GetNumB4() const {
    return numB4;
  }

  size_t GetPosition() const {
    return position;
  }
  size_t GetRadix() const {
    return radix;
  }

  void SetNextPass(Pass<PR>* np) {
    nextPass = np;
  }
  void SetGrouping(bool grp) {
    enableGrouping = grp;
  }
  void GeneratePass(const hcfftPlanHandle plHandle, bool fwd, std::string &passStr, bool fft_3StepTwiddle,
                    bool twiddleFront, bool inInterleaved, bool outInterleaved, bool inReal, bool outReal,
                    size_t inStride, size_t outStride, double scale, size_t lWorkSize, size_t count,
                    bool gIn = false, bool gOut = false) const {
    const std::string bufferInRe  = (inReal || inInterleaved) ?   "bufIn"  : "bufInRe";
    const std::string bufferInIm  = (inReal || inInterleaved) ?   "bufIn"  : "bufInIm";
    const std::string bufferOutRe = (outReal || outInterleaved) ? "bufOut" : "bufOutRe";
    const std::string bufferOutIm = (outReal || outInterleaved) ? "bufOut" : "bufOutIm";
    const std::string bufferInRe2  = (inReal || inInterleaved) ?   "bufIn2"  : "bufInRe2";
    const std::string bufferInIm2  = (inReal || inInterleaved) ?   "bufIn2"  : "bufInIm2";
    const std::string bufferOutRe2 = (outReal || outInterleaved) ? "bufOut2" : "bufOutRe2";
    const std::string bufferOutIm2 = (outReal || outInterleaved) ? "bufOut2" : "bufOutIm2";
    std::string twType = RegBaseType<PR>(2);

    // for real transforms we use only B1 butteflies (regC = 1)
    if(r2c || c2r) {
      assert(numB1 == numButterfly);
      assert(linearRegs);
    }

    // Check if it is single pass transform
    bool singlePass = ((position == 0) && (nextPass == NULL));

    if(singlePass) {
      assert(numButterfly == 1);  // for single pass transforms, there can be only 1 butterfly per transform
    }

    if(singlePass) {
      assert(workGroupSize == 1);
    }

    // Register types
    std::string regB1Type = RegBaseType<PR>(1);
    std::string regB2Type = RegBaseType<PR>(2);
    std::string regB4Type = RegBaseType<PR>(4);
    //Function attribute
    passStr += "inline void\n";
    //Function name
    passStr += PassName(count, position, fwd);
    // Function arguments
    passStr += "(";
    passStr += "unsigned int rw, unsigned int b, ";

    if(realSpecial) {
      passStr += "uint t, ";
    }

    passStr += "unsigned int me, unsigned int inOffset, unsigned int outOffset, ";

    // For now, interleaved support is there for only global buffers
    // TODO : add support for LDS interleaved
    //  if(inInterleaved)  assert(gIn);
    //  if(outInterleaved) assert(gOut);

    if(r2c || c2r) {
			assert(halfLds);
      if(gIn) {
        if(inInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";

          if(!rcSimple) {
            passStr += regB2Type;
            passStr += " *";
            passStr += bufferInRe2;
            passStr += ", ";
          }
        } else if(inReal) {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";

          if(!rcSimple) {
            passStr += regB1Type;
            passStr += " *";
            passStr += bufferInRe2;
            passStr += ", ";
          }
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";

          if(!rcSimple) {
            passStr += regB1Type;
            passStr += " *";
            passStr += bufferInRe2;
            passStr += ", ";
            passStr += "unsigned int iOffset2,";
          }

          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInIm;
          passStr += ", ";

            if(!rcSimple) {
              passStr += regB1Type;
              passStr += " *";
              passStr += bufferInIm2;
              passStr += ", ";
            }

        }
      } else {
        passStr += regB1Type;
        passStr += " *";
        passStr += bufferInRe;
        passStr += ", ";
        passStr += regB1Type;
        passStr += " *";
        passStr += bufferInIm;
        passStr += ", ";
      }

      if(gOut) {
        if(outInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferOutRe;

          if(!rcSimple) {
            passStr += ", ";
            passStr += regB2Type;
            passStr += " *";
            passStr += bufferOutRe2;
          }
        } else if(outReal) {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutRe;

          if(!rcSimple) {
            passStr += ", ";
            passStr += regB1Type;
            passStr += " *";
            passStr += bufferOutRe2;
          }
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutRe;
          passStr += ", ";

          if(!rcSimple) {
            passStr += regB1Type;
            passStr += " *";
            passStr += bufferOutRe2;
            passStr += ", ";
          }

          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutIm;

          if(!rcSimple) {
            passStr += ", ";
            passStr += regB1Type;
            passStr += " *";
            passStr += bufferOutIm2;
          }
        }
      } else {
        passStr += regB1Type;
        passStr += " *";
        passStr += bufferOutRe;
        passStr += ", ";
        passStr += regB1Type;
        passStr += " *";
        passStr += bufferOutIm;
      }
    } else {
      if(gIn) {
        if(inInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInIm;
          passStr += ", ";
        }
      } else {
        if(inInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInRe;
          passStr += ", ";
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferInIm;
          passStr += ", ";
        }
      }

      if(gOut) {
        if(outInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferOutRe;
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutRe;
          passStr += ", ";
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutIm;
        }
      } else {
        if(outInterleaved) {
          passStr += regB2Type;
          passStr += " *";
          passStr += bufferOutRe;
        } else {
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutRe;
          passStr += ", ";
          passStr += regB1Type;
          passStr += " *";
          passStr += bufferOutIm;
        }
      }
    }

    // Register arguments
    if(linearRegs) {
      passStr += ", ";
      passStr += IterRegArgs();
    }

    if(length > 1) {
      passStr += ", ";
      passStr += twType;
      passStr += " *";
      passStr += TwTableName();
    }

    if(fft_3StepTwiddle) {
      passStr += ", ";
      passStr += twType;
      passStr += " *";
      passStr += TwTableLargeName();
    }

    passStr += ", hc::tiled_index<2> &tidx) [[hc]]\n{\n";

    // Register Declarations
    if(!linearRegs) {
      DeclareRegs(regB1Type, 1, numB1, passStr);
      DeclareRegs(regB2Type, 2, numB2, passStr);
      DeclareRegs(regB4Type, 4, numB4, passStr);
    }

    // odd cnPerWI processing
    bool oddp = false;
    oddp = ((cnPerWI % 2) && (length > 1) && (!singlePass));

    // additional register for odd
    if( !rcSimple && oddp && ((r2c && (nextPass == NULL)) || (c2r && (position == 0))) ) {
      passStr += "\n\t";
      passStr += "uint brv = 0;\n\t";
      passStr += "\n\t";
      passStr += regB2Type;
      passStr += " R";
      passStr += SztToStr(cnPerWI);
      passStr += "[1];\n\t";
      passStr += "(R";
      passStr += SztToStr(cnPerWI);
      passStr += "[0]).x = 0; ";
      passStr += "(R";
      passStr += SztToStr(cnPerWI);
      passStr += "[0]).y = 0;\n";
    }

    // Special private memory for c-r 1 pass transforms
    if( !rcSimple && (c2r && (position == 0)) && singlePass ) {
      assert(radix == length);
      passStr += "\n\t";
      passStr += regB1Type;
      passStr += " mpvt[";
      passStr += SztToStr(length);
      passStr += "];\n";
    }

    passStr += "\n";

    // Read into registers
    if(r2c) {
      if(position == 0) {
        passStr += "\n\tif(rw)\n\t{";

        SweepRegs(plHandle, SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 1, numB1, 0, passStr);

        passStr += "\n\t}\n";

        if(rcSimple) {
          passStr += "\n";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, true, true, false, bufferInRe2, bufferInIm2, "inOffset", passStr);

          passStr += "\n";
        } else {
          passStr += "\n\tif(rw > 1)\n\t{";
          SweepRegs(plHandle, SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, bufferInRe2, bufferInIm2, "inOffset", 1, numB1, 0, passStr);

          passStr += "\n\t}\n";
          passStr += "\telse\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, true, true, false, bufferInRe2, bufferInIm2, "inOffset", passStr);

          passStr += "\n\t}\n";
        }
      }
    } else if(c2r && !rcSimple) {
      if(position == 0) {
        std::string processBufRe = bufferOutRe;
        std::string processBufIm = bufferOutIm;
        std::string processBufOffset = "outOffset";
        size_t processBufStride = outStride;

        if(singlePass) {
          processBufRe = "mpvt";
          processBufIm = "mpvt";
          processBufOffset = "0";
          processBufStride = 1;
        }

        passStr += "\n\tif(rw && !me)\n\t{\n\t";
        passStr += processBufRe;
        passStr += "[";
        passStr += processBufOffset;
        passStr += "] = ";

        passStr += bufferInRe;
        passStr += "[inOffset]";

        if(inInterleaved) {
          passStr += ".x;\n\t}";
        } else {
          passStr += ";\n\t}";
        }

        if(length > 1) {
          passStr += "\n\n\tif(rw)\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, false, false, bufferInRe, bufferInRe, "inOffset", passStr);

          passStr += "\n\t}\n";
          passStr += "\n\tif(rw > 1)\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, true, false, bufferInIm2, bufferInIm2, "inOffset", passStr);

          passStr += "\n\t}\n\telse\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, true, true, false, bufferInIm2, bufferInIm2, "inOffset", passStr);

          passStr += "\n\t}\n";

          if(oddp) {
            passStr += "\n\tif(rw && (me%2))\n\t{";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, false, true, bufferInRe, bufferInRe, "inOffset", passStr);

            passStr += "\n\t}";
            passStr += "\n\tif((rw > 1) && (me%2))\n\t{";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, true, true, bufferInIm2, bufferInIm2, "inOffset", passStr);

            passStr += "\n\t}\n";
          }

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_REAL, 1.0f, false, true, false, processBufRe, processBufIm, processBufOffset, passStr);

          if(oddp) {
            passStr += "\n\tif(me%2)\n\t{";
            SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_REAL, 1.0f, false, true, true, processBufRe, processBufIm, processBufOffset, passStr);
            passStr += "\n\t}\n";
          }

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_REAL, 1.0f, false, false, false, processBufRe, processBufIm, processBufOffset, passStr);

          if(oddp) {
            passStr += "\n\tif(me%2)\n\t{";
            SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_REAL, 1.0f, false, false, true, processBufRe, processBufIm, processBufOffset, passStr);
            passStr += "\n\t}\n";
          }
        }

        passStr += "\n\n\ttidx.barrier.wait_with_tile_static_memory_fence();\n";
        SweepRegs(plHandle, SR_READ, fwd, outInterleaved, processBufStride, SR_COMP_REAL, 1.0f, false, processBufRe, processBufIm, processBufOffset, 1, numB1, 0, passStr, false, oddp);
        passStr += "\n\n\ttidx.barrier.wait_with_tile_static_memory_fence();\n";
        passStr += "\n\tif((rw > 1) && !me)\n\t{\n\t";
        passStr += processBufIm;
        passStr += "[";
        passStr += processBufOffset;
        passStr += "] = ";

        passStr += bufferInRe2;
        passStr += "[inOffset]";

        if(inInterleaved) {
          passStr += ".x;\n\t}";
        } else {
          passStr += ";\n\t}";
        }

        passStr += "\n\tif((rw == 1) && !me)\n\t{\n\t";
        passStr += processBufIm;
        passStr += "[";
        passStr += processBufOffset;
        passStr += "] = 0;\n\t}";

        if(length > 1) {
          passStr += "\n\n\tif(rw)\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, false, false, bufferInIm, bufferInIm, "inOffset", passStr);

          passStr += "\n\t}\n";
          passStr += "\n\tif(rw > 1)\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, true, false, bufferInRe2, bufferInRe2, "inOffset", passStr);

          passStr += "\n\t}\n\telse\n\t{";

          SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, true, true, false, bufferInRe2, bufferInRe2, "inOffset", passStr);

          passStr += "\n\t}\n";

          if(oddp) {
            passStr += "\n\tif(rw && (me%2))\n\t{";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, false, true, bufferInIm, bufferInIm, "inOffset", passStr);

            passStr += "\n\t}";
            passStr += "\n\tif((rw > 1) && (me%2))\n\t{";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, true, true, bufferInRe2, bufferInRe2, "inOffset", passStr);

            passStr += "\n\t}";
          }
					passStr += "\n";

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_IMAG, 1.0f, false, true, false, processBufRe, processBufIm, processBufOffset, passStr);

          if(oddp) {
            passStr += "\n\tif(me%2)\n\t{";
            SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_IMAG, 1.0f, false, true, true, processBufRe, processBufIm, processBufOffset, passStr);
            passStr += "\n\t}\n";
          }

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_IMAG, 1.0f, false, false, false, processBufRe, processBufIm, processBufOffset, passStr);

          if(oddp) {
            passStr += "\n\tif(me%2)\n\t{";
            SweepRegsRC(SR_WRITE, fwd, outInterleaved, processBufStride, SR_COMP_IMAG, 1.0f, false, false, true, processBufRe, processBufIm, processBufOffset, passStr);
            passStr += "\n\t}\n";
          }
        }

        passStr += "\n\n\ttidx.barrier.wait_with_tile_static_memory_fence();\n";
        SweepRegs(plHandle, SR_READ, fwd, outInterleaved, processBufStride, SR_COMP_IMAG, 1.0f, false, processBufRe, processBufIm, processBufOffset, 1, numB1, 0, passStr);
        passStr += "\n\n\ttidx.barrier.wait_with_tile_static_memory_fence();\n";
      }
    } else {
      if( (!halfLds) || (halfLds && (position == 0)) ) {
	bool isPrecallVector = false;

        passStr += "\n\tif(rw)\n\t{";

        SweepRegs(plHandle, SR_READ, fwd, inInterleaved, inStride, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 1, numB1, 0, passStr, isPrecallVector);
        SweepRegs(plHandle, SR_READ, fwd, inInterleaved, inStride, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 2, numB2, numB1, passStr, isPrecallVector);
        SweepRegs(plHandle, SR_READ, fwd, inInterleaved, inStride, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 4, numB4, 2 * numB2 + numB1, passStr, isPrecallVector);

        passStr += "\n\t}\n";
      }
    }

    passStr += "\n";
    // 3-step twiddle multiplies done in the front
    bool tw3Done = false;

    if(fft_3StepTwiddle && twiddleFront) {
      tw3Done = true;

      if(linearRegs) {
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, true, bufferInRe, bufferInIm, "", 1, numB1, 0, passStr);
      } else {
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, true, bufferInRe, bufferInIm, "", 1, numB1, 0, passStr);
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, true, bufferInRe, bufferInIm, "", 2, numB2, numB1, passStr);
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, true, bufferInRe, bufferInIm, "", 4, numB4, 2 * numB2 + numB1, passStr);
      }
    }

    passStr += "\n";

    // Twiddle multiply
    if( (position > 0) && (radix > 1) ) {
      SweepRegs(plHandle, SR_TWMUL, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 1, numB1, 0, passStr);
      SweepRegs(plHandle, SR_TWMUL, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 2, numB2, numB1, passStr);
      SweepRegs(plHandle, SR_TWMUL, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 4, numB4, 2 * numB2 + numB1, passStr);
    }

    // Butterfly calls
    if(radix > 1) {
      if(numB1) {
        CallButterfly(ButterflyName(radix, 1, fwd, count), 1, numB1, passStr);
      }

      if(numB2) {
        CallButterfly(ButterflyName(radix, 2, fwd, count), 2, numB2, passStr);
      }

      if(numB4) {
        CallButterfly(ButterflyName(radix, 4, fwd, count), 4, numB4, passStr);
      }
    }

    passStr += "\n";

    if(!halfLds) passStr += "\n\n\ttidx.barrier.wait_with_tile_static_memory_fence();\n\n\n";

    // 3-step twiddle multiplies
    if(fft_3StepTwiddle && !tw3Done) {
      assert(nextPass == NULL);

      if(linearRegs) {
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 1, numB1, 0, passStr);
      } else {
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 1, numB1, 0, passStr);
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 2, numB2, numB1, passStr);
        SweepRegs(plHandle, SR_TWMUL_3STEP, fwd, false, 1, SR_COMP_BOTH, 1.0f, false, bufferInRe, bufferInIm, "", 4, numB4, 2 * numB2 + numB1, passStr);
      }
    }

    // Write back from registers
    if(halfLds) {
      // In this case, we have to write & again read back for the next pass since we are
      // using only half the lds. Number of barriers will increase at the cost of halving the lds.
      if(nextPass == NULL) { // last pass
        if(r2c && !rcSimple) {
          if(!singlePass) {
            SweepRegs(plHandle, SR_WRITE, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 1, numB1, 0, passStr);

            passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, false, false, bufferInRe, bufferInIm, "inOffset", passStr);

            if(oddp) {
              passStr += "\n\tif(me%2)\n\t{";

              SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_REAL, 1.0f, false, false, true, bufferInRe, bufferInIm, "inOffset", passStr);

              passStr += "\n\t}\n";
            }

            passStr += "\n\n\n\tif(rw && !me)\n\t{\n\t";

            if(outInterleaved) {
              passStr += bufferOutRe;
              passStr += "[outOffset].x = ";

              passStr += bufferInRe;
              passStr += "[inOffset]";

              if(scale != 1.0) {
                passStr += " * ";
                passStr += FloatToStr(scale);
                passStr += FloatSuffix<PR>();
              }

              passStr += ";\n\t";

              passStr += bufferOutIm;
              passStr += "[outOffset].y = ";
              passStr += "0;\n\t}";
            } else {
                passStr += bufferOutRe;
                passStr += "[outOffset] = ";

                passStr += bufferInRe;
                passStr += "[inOffset]";

                if(scale != 1.0) {
                  passStr += " * ";
                  passStr += FloatToStr(scale);
                  passStr += FloatSuffix<PR>();
                }

                passStr += ";\n\t";

                passStr += bufferOutIm;
                passStr += "[outOffset] = ";
                passStr += "0;\n\t}";
            }

            passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";

            SweepRegs(plHandle, SR_WRITE, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, bufferInRe, bufferInIm, "inOffset", 1, numB1, 0, passStr);

            passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";

            SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, false, false, bufferInRe, bufferInIm, "inOffset", passStr);

            if(oddp) {
              passStr += "\n\tif(me%2)\n\t{";

              SweepRegsRC(SR_READ, fwd, inInterleaved, inStride, SR_COMP_IMAG, 1.0f, false, false, true, bufferInRe, bufferInIm, "inOffset", passStr);

              passStr += "\n\t}\n";
            }

            passStr += "\n\tif((rw > 1) && !me)\n\t{\n\t";

            if(outInterleaved) {
              passStr += bufferOutRe2;
              passStr += "[outOffset].x = ";

              passStr += bufferInIm;
              passStr += "[inOffset]";

              if(scale != 1.0) {
                passStr += " * ";
                passStr += FloatToStr(scale);
                passStr += FloatSuffix<PR>();
              }

              passStr += ";\n\t";

              passStr += bufferOutIm2;
              passStr += "[outOffset].y = ";
              passStr += "0;\n\t}";
            } else {
                passStr += bufferOutRe2;
                passStr += "[outOffset] = ";

                passStr += bufferInIm;
                passStr += "[inOffset]";

              if(scale != 1.0) {
                passStr += " * ";
                passStr += FloatToStr(scale);
                passStr += FloatSuffix<PR>();
              }

              passStr += ";\n\t";

              passStr += bufferOutIm2;
              passStr += "[outOffset] = ";
              passStr += "0;\n\t}";
            }

            passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";
          }

          passStr += "\n\n\tif(rw)\n\t{";

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, false, false, bufferOutRe, bufferOutIm, "outOffset", passStr);

          passStr += "\n\t}\n";

          if(oddp) {
            passStr += "\n\n\tbrv = ((rw != 0) & (me%2 == 1));\n\t";
            passStr += "if(brv)\n\t{";

            SweepRegsRC(SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, false, true, bufferOutRe, bufferOutIm, "outOffset", passStr);

            passStr += "\n\t}\n";
          }

          passStr += "\n\n\tif(rw > 1)\n\t{";

          SweepRegsRC(SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, true, false, bufferOutRe2, bufferOutIm2, "outOffset", passStr);

          passStr += "\n\t}\n";

          if(oddp) {
            passStr += "\n\n\tbrv = ((rw > 1) & (me%2 == 1));\n\t";
            passStr += "if(brv)\n\t{";

            SweepRegsRC(SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, true, true, bufferOutRe2, bufferOutIm2, "outOffset", passStr);

            passStr += "\n\t}\n";
          }
        } else if(c2r) {
          passStr += "\n\tif(rw)\n\t{";

          SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_REAL, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, numB1, 0, passStr);

          passStr += "\n\t}\n";

          if(!rcSimple) {
            passStr += "\n\tif(rw > 1)\n\t{";

            SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_IMAG, scale, false, bufferOutRe2, bufferOutIm2, "outOffset", 1, numB1, 0, passStr);

            passStr += "\n\t}\n";
          }
        } else {
          passStr += "\n\tif(rw)\n\t{";

          SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, numB1, 0, passStr);

          passStr += "\n\t}\n";
        }
      } else {
        passStr += "\n\tif(rw)\n\t{";

        SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_REAL, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, numB1, 0, passStr);

        passStr += "\n\t}\n";
        passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";
        passStr += "\n\tif(rw)\n\t{";

        nextPass->SweepRegs(plHandle, SR_READ, fwd, outInterleaved, outStride, SR_COMP_REAL, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, nextPass->GetNumB1(), 0, passStr);

        passStr += "\n\t}\n";
        passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";
        passStr += "\n\tif(rw)\n\t{";

        SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_IMAG, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, numB1, 0, passStr);

        passStr += "\n\t}\n";
        passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";
        passStr += "\n\tif(rw)\n\t{";

        nextPass->SweepRegs(plHandle, SR_READ, fwd, outInterleaved, outStride, SR_COMP_IMAG, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, nextPass->GetNumB1(), 0, passStr);

        passStr += "\n\t}\n";
        passStr += "\n\ntidx.barrier.wait_with_tile_static_memory_fence();\n";
      }
    } else {
      passStr += "\n\tif(rw)\n\t{";

      SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, bufferOutRe, bufferOutIm, "outOffset", 1, numB1, 0, passStr);
      SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, bufferOutRe, bufferOutIm, "outOffset", 2, numB2, numB1, passStr);
      SweepRegs(plHandle, SR_WRITE, fwd, outInterleaved, outStride, SR_COMP_BOTH, scale, false, bufferOutRe, bufferOutIm, "outOffset", 4, numB4, 2 * numB2 + numB1, passStr);

      passStr += "\n\t}\n";
    }

    passStr += "\n}\n\n";
  }
};

// FFT kernel
template <Precision PR>
class Kernel {
  size_t length;          // Length of FFT
  size_t workGroupSize;       // Work group size
  size_t cnPerWI;         // complex numbers per work-item

  size_t numTrans;        // Number of transforms per work-group
  size_t workGroupSizePerTrans;     // Work group subdivision per transform
  size_t numPasses;       // Number of FFT passes
  std::vector<size_t> radices;      // Base radix at each pass
  std::vector<Pass<PR> > passes;      // Array of pass objects

  bool halfLds;         // LDS used to store one component (either real or imaginary) at a time
  // for passing intermediate data between the passes, if this is set
  // then each pass-function should accept same set of registers

  bool linearRegs;
  // Future optimization ideas
  // bool limitRegs;        // TODO: Incrementally write to LDS, thereby using same set of registers for more than 1 butterflies
  // bool combineReadTwMul;     // TODO: Combine reading into registers and Twiddle multiply

  bool r2c2r;         // real to complex or complex to real transform
  bool r2c, c2r;
  bool rcFull;
  bool rcSimple;

  bool blockCompute;            // When we have to compute FFT in blocks (either read or write is along columns)
  BlockComputeType blockComputeType;
  size_t blockWidth, blockWGS, blockLDS;

  bool realSpecial;

  const FFTKernelGenKeyParams params;   // key params

  inline std::string IterRegs(const std::string &pfx, bool initComma = true) {
    std::string str = "";

    if(linearRegs) {
      if(initComma) {
        str += ", ";
      }

      for(size_t i = 0; i < cnPerWI; i++) {
        if(i != 0) {
          str += ", ";
        }

        str += pfx;
        str += "R";
        str += SztToStr(i);
      }
    }

    return str;
  }

  inline bool IsGroupedReadWritePossible() {
    bool possible = true;
    const size_t* iStride, *oStride;

    if(r2c2r) {
      return false;
    }

    if(realSpecial) {
      return false;
    }

    if(params.fft_placeness == HCFFT_INPLACE) {
      iStride = oStride = params.fft_inStride;
    } else {
      iStride = params.fft_inStride;
      oStride = params.fft_outStride;
    }

    for(size_t i = 1; i < params.fft_DataDim; i++) {
      if(iStride[i] % 2) {
        possible = false;
        break;
      }

      if(oStride[i] % 2) {
        possible = false;
        break;
      }
    }

    return possible;
  }

  inline std::string OffsetCalcBlock(const std::string &off, bool input = true) {
    std::string str;
    const size_t* pStride = input ? params.fft_inStride : params.fft_outStride;
    str += "\t";
    str += off;
    str += " = ";
    std::string nextBatch = "batch";

    for(size_t i = (params.fft_DataDim - 1); i > 2; i--) {
      size_t currentLength = 1;

      for(int j = 2; j < i; j++) {
        currentLength *= params.fft_N[j];
      }

      currentLength *= (params.fft_N[1] / blockWidth);
      str += "(";
      str += nextBatch;
      str += "/";
      str += SztToStr(currentLength);
      str += ")*";
      str += SztToStr(pStride[i]);
      str += " + ";
      nextBatch = "(" + nextBatch + "%" + SztToStr(currentLength) + ")";
    }

    str += "(";
    str += nextBatch;
    str += "/";
    str += SztToStr(params.fft_N[1] / blockWidth);
    str += ")*";
    str += SztToStr(pStride[2]);
    str += " + (";
    str += nextBatch;
    str += "%";
    str += SztToStr(params.fft_N[1] / blockWidth);
    str += ")*";

    if( (input && (blockComputeType == BCT_R2C)) || (!input && (blockComputeType == BCT_C2R)) ) {
      str += SztToStr(blockWidth * length);
    } else {
      str += SztToStr(blockWidth);
    }

    str += ";\n";
    return str;
  }

  inline std::string OffsetCalc(const std::string &off, bool input = true, bool rc_second_index = false) {
    std::string str;
    const size_t* pStride = input ? params.fft_inStride : params.fft_outStride;
    std::string batch;

    if(r2c2r && !rcSimple) {
      batch += "(batch*";
      batch += SztToStr(2 * numTrans);

      if(rc_second_index) {
        batch += " + 1";
      } else {
        batch += " + 0";
      }

      if(numTrans != 1) {
        batch += " + 2*(me/";
        batch += SztToStr(workGroupSizePerTrans);
        batch += "))";
      } else        {
        batch += ")";
      }
    } else {
      if(numTrans == 1) {
        batch += "batch";
      } else      {
        batch += "(batch*";
        batch += SztToStr(numTrans);
        batch += " + (me/";
        batch += SztToStr(workGroupSizePerTrans);
        batch += "))";
      }
    }

    str += "\t";
    str += off;
    str += " = ";
    std::string nextBatch = batch;

    for(size_t i = (params.fft_DataDim - 1); i > 1; i--) {
      size_t currentLength = 1;

      for(int j = 1; j < i; j++) {
        currentLength *= params.fft_N[j];
      }

      str += "(";
      str += nextBatch;
      str += "/";
      str += SztToStr(currentLength);
      str += ")*";
      str += SztToStr(pStride[i]);
      str += " + ";
      nextBatch = "(" + nextBatch + "%" + SztToStr(currentLength) + ")";
    }

    str += nextBatch;
    str += "*";
    str += SztToStr(pStride[1]);
    str += ";\n";
    return str;
  }

 public:
  Kernel( const FFTKernelGenKeyParams &paramsVal) :
    r2c2r(false), params(paramsVal)

  {
    length = params.fft_N[0];
    workGroupSize = params.fft_SIMD;
    numTrans = (workGroupSize * params.fft_R) / length;
    r2c = false;
    c2r = false;

    // Check if it is R2C or C2R transform
    if(params.fft_inputLayout == HCFFT_REAL) {
      r2c = true;
    }

    if(params.fft_outputLayout == HCFFT_REAL) {
      c2r = true;
    }

    r2c2r = (r2c || c2r);

    if(r2c) {
      rcFull = ((params.fft_outputLayout == HCFFT_COMPLEX_INTERLEAVED) ||
                (params.fft_outputLayout == HCFFT_COMPLEX_PLANAR) ) ? true : false;
    }

    if(c2r) {
      rcFull = ((params.fft_inputLayout  == HCFFT_COMPLEX_INTERLEAVED) ||
                (params.fft_inputLayout  == HCFFT_COMPLEX_PLANAR) ) ? true : false;
    }

    rcSimple = params.fft_RCsimple;

		halfLds = true;
		linearRegs = true;

    realSpecial = params.fft_realSpecial;

    blockCompute = params.blockCompute;
    blockComputeType = params.blockComputeType;

    // Make sure we can utilize all Lds if we are going to
    // use blocked columns to compute FFTs
    if(blockCompute) {
      assert(length <= 256);  // 256 parameter comes from prototype experiments
      // largest length at which block column possible given 32KB LDS limit
      // if LDS limit is different this number need to be changed appropriately
      halfLds = false;
      linearRegs = true;
    }

    assert( ((length * numTrans) % workGroupSize) == 0 );
    cnPerWI = (numTrans * length) / workGroupSize;
    workGroupSizePerTrans = workGroupSize / numTrans;
    // !!!! IMPORTANT !!!! Keep these assertions unchanged, algorithm depend on these to be true
    assert( (cnPerWI * workGroupSize) == (numTrans * length) );
    assert( cnPerWI <= length ); // Don't do more than 1 fft per work-item
    // Breakdown into passes
    size_t LS = 1;
    size_t L;
    size_t R = length;
    size_t pid = 0;
    // See if we can get radices from the lookup table
    const size_t* pRadices = NULL;
    size_t nPasses;
    KernelCoreSpecs<PR> kcs;
    kcs.GetRadices(length, nPasses, pRadices);

    if((params.fft_MaxWorkGroupSize >= 256) && (pRadices != NULL)) {
      for(size_t i = 0; i < nPasses; i++) {
        size_t rad = pRadices[i];
        L = LS * rad;
        R /= rad;
        radices.push_back(rad);
        passes.push_back(Pass<PR>(i, length, rad, cnPerWI, L, LS, R, linearRegs, halfLds, r2c, c2r, rcFull, rcSimple, realSpecial));
        LS *= rad;
      }

      assert(R == 1); // this has to be true for correct radix composition of the length
      numPasses = nPasses;
    } else {
      // Possible radices
      size_t cRad[] = {13, 11, 10, 8, 7, 6, 5, 4, 3, 2, 1}; // Must be in descending order
      size_t cRadSize = (sizeof(cRad) / sizeof(cRad[0]));

      while(true) {
        size_t rad;
        assert(cRadSize >= 1);

        for(size_t r = 0; r < cRadSize; r++) {
          rad = cRad[r];

          if((rad > cnPerWI) || (cnPerWI % rad)) {
            continue;
          }

          if(!(R % rad)) {
            break;
          }
        }

        assert((cnPerWI % rad) == 0);
        L = LS * rad;
        R /= rad;
        radices.push_back(rad);
        passes.push_back(Pass<PR>(pid, length, rad, cnPerWI, L, LS, R, linearRegs, halfLds, r2c, c2r, rcFull, rcSimple, realSpecial));
        pid++;
        LS *= rad;
        assert(R >= 1);

        if(R == 1) {
          break;
        }
      }

      numPasses = pid;
    }

    assert(numPasses == passes.size());
    assert(numPasses == radices.size());
    // Grouping read/writes ok?
    bool grp = IsGroupedReadWritePossible();

    for(size_t i = 0; i < numPasses; i++) {
      passes[i].SetGrouping(grp);
    }

    // Store the next pass-object pointers
    if(numPasses > 1)
      for(size_t i = 0; i < (numPasses - 1); i++) {
        passes[i].SetNextPass(&passes[i + 1]);
      }

    if(blockCompute) {
      blockWidth = BlockSizes::BlockWidth(length);
      blockWGS = BlockSizes::BlockWorkGroupSize(length);
      blockLDS = BlockSizes::BlockLdsSize(length);
    } else {
      blockWidth = blockWGS = blockLDS = 0;
    }
  }

  class BlockSizes {
   public:
    enum ValType {
      BS_VT_WGS,
      BS_VT_BWD,
      BS_VT_LDS,
    };

    static size_t BlockLdsSize(size_t N) {
      return GetValue(N, BS_VT_LDS);
    }
    static size_t BlockWidth(size_t N) {
      return GetValue(N, BS_VT_BWD);
    }
    static size_t BlockWorkGroupSize(size_t N) {
      return GetValue(N, BS_VT_WGS);
    }

   private:

    static size_t GetValue(size_t N, ValType vt) {
      size_t wgs; // preferred work group size
      size_t bwd; // block width to be used
      size_t lds; // LDS size to be used for the block
      KernelCoreSpecs<PR> kcs;
      size_t t_wgs, t_nt;
      kcs.GetWGSAndNT(N, t_wgs, t_nt);

      switch(N) {
        case 256:
          bwd = 8 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 256 : t_wgs;
          break;

        case 128:
          bwd = 8 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 128 : t_wgs;
          break;

        case 64:
          bwd = 16 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 128 : t_wgs;
          break;

        case 32:
          bwd = 32 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 64  : t_wgs;
          break;

        case 16:
          bwd = 64 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 64  : t_wgs;
          break;

        case 8:
          bwd = 128 / PrecisionWidth<PR>();
          wgs = (bwd > t_nt) ? 64  : t_wgs;
          break;

        default:
          assert(false);
      }

      // block width cannot be less than numTrans, math in other parts of code depend on this assumption
      assert(bwd >= t_nt);
      lds = N * bwd;

      switch(vt) {
        case BS_VT_WGS:
          return wgs;

        case BS_VT_BWD:
          return bwd;

        case BS_VT_LDS:
          return lds;

        default:
          assert(false);
          return 0;
      }
    }
  };

  void GenerateKernel(void **twiddles, void **twiddleslarge, accelerator acc, const hcfftPlanHandle plHandle, std::string &str, vector< size_t > gWorkSize, vector< size_t > lWorkSize, size_t count) {
    std::string twType = RegBaseType<PR>(2);
    std::string rType  = RegBaseType<PR>(1);
    std::string r2Type  = RegBaseType<PR>(2);
    bool inInterleaved;  // Input is interleaved format
    bool outInterleaved; // Output is interleaved format
    inInterleaved  = ((params.fft_inputLayout == HCFFT_COMPLEX_INTERLEAVED) ||
                      (params.fft_inputLayout == HCFFT_HERMITIAN_INTERLEAVED) ) ? true : false;
    outInterleaved = ((params.fft_outputLayout == HCFFT_COMPLEX_INTERLEAVED) ||
                      (params.fft_outputLayout == HCFFT_HERMITIAN_INTERLEAVED) ) ? true : false;
    // use interleaved LDS when halfLds constraint absent
    bool ldsInterleaved = inInterleaved || outInterleaved;
    ldsInterleaved = halfLds ? false : ldsInterleaved;
    ldsInterleaved = blockCompute ? true : ldsInterleaved;
    bool inReal;  // Input is real format
    bool outReal; // Output is real format
    inReal  = (params.fft_inputLayout == HCFFT_REAL) ? true : false;
    outReal = (params.fft_outputLayout == HCFFT_REAL) ? true : false;
    size_t large1D = 0;

    if(params.fft_realSpecial) {
      large1D = params.fft_N[0] * params.fft_realSpecial_Nr;
    } else {
      large1D = params.fft_N[0] * params.fft_N[1];
    }

    std::string sfx = FloatSuffix<PR>() + "\n";

    // Base type
    str += "#define fptype "; str += RegBaseType<PR>(1); str += "\n\n";

    // Vector type
    str += "#define fvect2 ";
    str += RegBaseType<PR>(2);
    str += "\n\n";

    //constants
    if (length%8 == 0)
    {
	str += "#define C8Q  0.70710678118654752440084436210485"; str += sfx; str += "\n";
    }
    if (length % 5 == 0)
    {
	str += "#define C5QA 0.30901699437494742410229341718282"; str += sfx; str += "\n";
	str += "#define C5QB 0.95105651629515357211643933337938"; str += sfx; str += "\n";
	str += "#define C5QC 0.50000000000000000000000000000000"; str += sfx; str += "\n";
	str += "#define C5QD 0.58778525229247312916870595463907"; str += sfx; str += "\n";
	str += "#define C5QE 0.80901699437494742410229341718282"; str += sfx; str += "\n";
    }
    if (length % 3 == 0)
    {
	str += "#define C3QA 0.50000000000000000000000000000000"; str += sfx; str += "\n";
	str += "#define C3QB 0.86602540378443864676372317075294"; str += sfx; str += "\n";
    }
    if (length % 7 == 0)
    {
	str += "#define C7Q1 -1.16666666666666651863693004997913"; str += sfx; str += "\n";
	str += "#define C7Q2  0.79015646852540022404554065360571"; str += sfx; str += "\n";
	str += "#define C7Q3  0.05585426728964774240049351305970"; str += sfx; str += "\n";
	str += "#define C7Q4  0.73430220123575240531721419756650"; str += sfx; str += "\n";
	str += "#define C7Q5  0.44095855184409837868031445395900"; str += sfx; str += "\n";
	str += "#define C7Q6  0.34087293062393136944265847887436"; str += sfx; str += "\n";
	str += "#define C7Q7 -0.53396936033772524066165487965918"; str += sfx; str += "\n";
	str += "#define C7Q8  0.87484229096165666561546458979137"; str += sfx; str += "\n";
    }

    if (length % 11 == 0)
    {
	str += "#define b11_0 0.9898214418809327"; str += sfx; str += "\n";
	str += "#define b11_1 0.9594929736144973"; str += sfx; str += "\n";
	str += "#define b11_2 0.9189859472289947"; str += sfx; str += "\n";
	str += "#define b11_3 0.8767688310025893"; str += sfx; str += "\n";
	str += "#define b11_4 0.8308300260037728"; str += sfx; str += "\n";
	str += "#define b11_5 0.7784344533346518"; str += sfx; str += "\n";
	str += "#define b11_6 0.7153703234534297"; str += sfx; str += "\n";
	str += "#define b11_7 0.6343562706824244"; str += sfx; str += "\n";
	str += "#define b11_8 0.3425847256816375"; str += sfx; str += "\n";
	str += "#define b11_9 0.5211085581132027"; str += sfx; str += "\n";
    }
    if (length % 13 == 0)
    {
	str += "#define b13_0  0.9682872443619840"; str += sfx; str += "\n";
	str += "#define b13_1  0.9578059925946651"; str += sfx; str += "\n";
	str += "#define b13_2  0.8755023024091479"; str += sfx; str += "\n";
	str += "#define b13_3  0.8660254037844386"; str += sfx; str += "\n";
	str += "#define b13_4  0.8595425350987748"; str += sfx; str += "\n";
	str += "#define b13_5  0.8534800018598239"; str += sfx; str += "\n";
	str += "#define b13_6  0.7693388175729806"; str += sfx; str += "\n";
	str += "#define b13_7  0.6865583707817543"; str += sfx; str += "\n";
	str += "#define b13_8  0.6122646503767565"; str += sfx; str += "\n";
	str += "#define b13_9  0.6004772719326652"; str += sfx; str += "\n";
	str += "#define b13_10 0.5817047785105157"; str += sfx; str += "\n";
	str += "#define b13_11 0.5751407294740031"; str += sfx; str += "\n";
	str += "#define b13_12 0.5220263851612750"; str += sfx; str += "\n";
	str += "#define b13_13 0.5200285718888646"; str += sfx; str += "\n";
	str += "#define b13_14 0.5165207806234897"; str += sfx; str += "\n";
	str += "#define b13_15 0.5149187780863157"; str += sfx; str += "\n";
	str += "#define b13_16 0.5035370328637666"; str += sfx; str += "\n";
	str += "#define b13_17 0.5000000000000000"; str += sfx; str += "\n";
	str += "#define b13_18 0.3027756377319946"; str += sfx; str += "\n";
	str += "#define b13_19 0.3014792600477098"; str += sfx; str += "\n";
	str += "#define b13_20 0.3004626062886657"; str += sfx; str += "\n";
	str += "#define b13_21 0.2517685164318833"; str += sfx; str += "\n";
	str += "#define b13_22 0.2261094450357824"; str += sfx; str += "\n";
	str += "#define b13_23 0.0833333333333333"; str += sfx; str += "\n";
	str += "#define b13_24 0.0386329546443481"; str += sfx; str += "\n";
    }

    str += "\n";

    bool cReg = linearRegs ? true : false;
    // Generate butterflies for all unique radices
    std::list<size_t> uradices;

    for(std::vector<size_t>::const_iterator r = radices.begin(); r != radices.end(); r++) {
      uradices.push_back(*r);
    }

    uradices.sort();
    uradices.unique();
    typename std::vector< Pass<PR> >::const_iterator p;

    if(length > 1) {
      for(std::list<size_t>::const_iterator r = uradices.begin(); r != uradices.end(); r++) {
        size_t rad = *r;
        p = passes.begin();

        while(p->GetRadix() != rad) {
          p++;
        }

        for(size_t d = 0; d < 2; d++) {
          bool fwd = d ? false : true;

          if(p->GetNumB1()) {
            Butterfly<PR> bfly(rad, 1, fwd, cReg);
            bfly.GenerateButterfly(str, count);
            str += "\n";
          }

          if(p->GetNumB2()) {
            Butterfly<PR> bfly(rad, 2, fwd, cReg);
            bfly.GenerateButterfly(str, count);
            str += "\n";
          }

          if(p->GetNumB4()) {
            Butterfly<PR> bfly(rad, 4, fwd, cReg);
            bfly.GenerateButterfly(str, count);
            str += "\n";
          }
        }
      }
    }

    if(PR == P_SINGLE)
    {
       TwiddleTableLarge<float_2, PR> twLarge(large1D);
       // twiddle factors for 1d-large 3-step algorithm
       if(params.fft_3StepTwiddle) {
         twLarge.GenerateTwiddleTable(str, plHandle);
         twLarge.TwiddleLargeAV(twiddleslarge, acc);
      }
    }
    else
    {
       TwiddleTableLarge<double_2, PR> twLarge(large1D);

       // twiddle factors for 1d-large 3-step algorithm
       if(params.fft_3StepTwiddle) {
         twLarge.GenerateTwiddleTable(str, plHandle);
         twLarge.TwiddleLargeAV(twiddleslarge, acc);
      }
    }

    // Generate passes
    for(size_t d = 0; d < 2; d++) {
      bool fwd;

      if(r2c2r) {
        fwd = r2c;
      } else {
        fwd = d ? false : true;
      }

      double scale = fwd ? params.fft_fwdScale : params.fft_backScale;

      for(p = passes.begin(); p != passes.end(); p++) {
        double s = 1.0;
        size_t ins = 1, outs = 1;
        bool gIn = false, gOut = false;
        bool inIlvd = false, outIlvd = false;
        bool inRl = false, outRl = false;
        bool tw3Step = false;

        if(p == passes.begin() && params.fft_twiddleFront ) {
          tw3Step = params.fft_3StepTwiddle;
        }

        if((p + 1) == passes.end()) {
          s = scale;

          if(!params.fft_twiddleFront) {
            tw3Step = params.fft_3StepTwiddle;
          }
        }

        if(blockCompute && !r2c2r) {
          inIlvd = ldsInterleaved;
          outIlvd = ldsInterleaved;
        } else {
          if(p == passes.begin())   {
            inIlvd  = inInterleaved;
            inRl  = inReal;
            gIn  = true;
            ins  = params.fft_inStride[0];
          }

          if((p + 1) == passes.end()) {
            outIlvd = outInterleaved;
            outRl = outReal;
            gOut = true;
            outs = params.fft_outStride[0];
          }

          if(p != passes.begin())   {
            inIlvd = ldsInterleaved;
          }

          if((p + 1) != passes.end()) {
            outIlvd = ldsInterleaved;
          }
        }

        p->GeneratePass(plHandle, fwd, str, tw3Step, params.fft_twiddleFront, inIlvd, outIlvd, inRl, outRl, ins, outs, s, lWorkSize[0], count, gIn, gOut);
      }

      // if real transform we do only 1 direction
      if(r2c2r) {
        break;
      }
    }

    // TODO : address this kludge

    for(size_t d = 0; d < 2; d++) {
      int arg = 0;
      bool fwd;

      if(r2c2r) {
        fwd = inReal ? true : false;
      } else {
        fwd = d ? false : true;
      }

      // FFT kernel begin
      str += "extern \"C\" {";
      str += "\nvoid ";

      // Function name
      if(fwd) {
        str += "fft_fwd";
        str += SztToStr(count);
      } else  {
        str += "fft_back";
        str += SztToStr(count);
      }

      str += "( std::map<int, void*> vectArr, uint batchSize, accelerator_view &acc_view, accelerator &acc )\n\t{\n\t";

      // Function attributes
      if(params.fft_placeness == HCFFT_INPLACE) {
        if(r2c2r) {
          if(outInterleaved) {
            str += r2Type;
            str += " *gb = static_cast<";
            str += r2Type;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gb = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }
        } else {
          assert(inInterleaved == outInterleaved);
          assert(params.fft_inStride[1] == params.fft_outStride[1]);
          assert(params.fft_inStride[0] == params.fft_outStride[0]);

          if(inInterleaved) {
            str += r2Type;
            str += " *gb = static_cast<";
            str += r2Type;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gbRe = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
            str += rType;
            str += " *gbIm = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }
        }
      } else {
        if(r2c2r) {
          if(inInterleaved) {
            str += r2Type;
            str += " *gbIn = static_cast<";
            str += r2Type;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else if(inReal) {
            str += rType;
            str += " *gbIn = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gbInRe = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
            str += rType;
            str += " *gbInIm = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }

          if(outInterleaved) {
            str += r2Type;
            str += " *gbOut = static_cast<";
            str += r2Type;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else if(outReal) {
            str += rType;
            str += " *gbOut = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gbOutRe = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
            str += rType;
            str += " *gbOutIm = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }
        } else {
          if(inInterleaved) {
            str += r2Type;
            str += " *gbIn = static_cast<";
            str += r2Type;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gbInRe = static_cast<";
            str += rType;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
            str += rType;
            str += " *gbInIm = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }

          if(outInterleaved) {
            str += r2Type;
            str += " *gbOut = static_cast<";
            str += r2Type;
            str += "*> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          } else {
            str += rType;
            str += " *gbOutRe = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
            str += rType;
            str += " *gbOutIm = static_cast<";
            str += rType;
            str += " *> (vectArr[";
            str += SztToStr(arg);
            str += "]);\n";
            arg++;
          }
        }
      }

      // Twiddle table
      if(length > 1 ) {
        str += "\n\n";
        str += r2Type;
        str += " *";
        str += TwTableName();
        str += " = static_cast< ";
        str += r2Type;
        str += " *> (vectArr[";
        str += SztToStr(arg);
        str += "]);\n";
        arg++;
        if(PR == P_SINGLE)
        {
          if ( d == 0 )
          {
            TwiddleTable<hc::short_vector::float_2> twTable(length);
            twTable.GenerateTwiddleTable(twiddles, acc, radices);
          }
        }
        else
        {
          if ( d == 0 )
          {
            TwiddleTable<hc::short_vector::double_2> twTable(length);
            twTable.GenerateTwiddleTable(twiddles, acc, radices);
          }
        }
      }

      str += "\n";

      // twiddle factors for 1d-large 3-step algorithm
      if(params.fft_3StepTwiddle) {
        str += "\n\n";
        str += r2Type;
        str += " *";
        str += TwTableLargeName();
        str += " = static_cast< ";
        str += r2Type;
        str += " *> (vectArr[";
        str += SztToStr(arg);
        str += "]);\n";
        arg++;
      }

      str += "\thc::extent<2> grdExt( ";
      str += SztToStr(gWorkSize[0]);
      str += ", 1 ); \n";
      str += "\thc::tiled_extent<2> t_ext = grdExt.tile(";
      str += SztToStr(lWorkSize[0]);
      str += ",1);\n";

      str += "\thc::parallel_for_each(acc_view, t_ext, [=] (hc::tiled_index<2> tidx) [[hc]]\n\t { ";

      // Initialize
      str += "\t";
      str += "unsigned int me = tidx.local[0];\n\t";
      str += "unsigned int batch = tidx.tile[0];";
      str += "\n";

      // Allocate LDS
      if(blockCompute) {
        str += "\n\t";
        str += "tile_static ";
        str += r2Type;
        str += " lds[";
        str += SztToStr(blockLDS);
        str += "];\n";
      } else {
        size_t ldsSize = halfLds ? length * numTrans : 2 * length * numTrans;
        ldsSize = ldsInterleaved ? ldsSize / 2 : ldsSize;

        if(numPasses > 1) {
          str += "\n\t";
          str += "tile_static ";
          str += ldsInterleaved ? r2Type : rType;
          str += " lds[";
          str += SztToStr(ldsSize);
          str += "];\n";
        }
      }

      // Declare memory pointers
      str += "\n\t";

      if(r2c2r) {
        str += "unsigned int iOffset;\n\t";
        str += "unsigned int oOffset;\n\n\t";

        if(!rcSimple) {
          str += "unsigned int iOffset2;\n\t";
          str += "unsigned int oOffset2;\n\n\t";
        }

	      if(inInterleaved)
	      {
	        if(!rcSimple)	{	str += r2Type; str += " *lwbIn2;\n\t"; }
	        str += r2Type; str += " *lwbIn;\n\t";
	      }
	      else if(inReal)
	      {
	        if(!rcSimple)	{	str += rType; str += " *lwbIn2;\n\t"; }
	        str += rType; str += " *lwbIn;\n\t";
	      }
	      else
	      {
	        if(!rcSimple)	{	str += rType; str += " *lwbInRe2;\n\t"; }
	        if(!rcSimple)	{	str += rType; str += " *lwbInIm2;\n\t"; }
	        str += rType; str += " *lwbInRe;\n\t";
	        str += rType; str += " *lwbInIm;\n\t";
	      }

	      if(outInterleaved)
	      {
          if(!rcSimple) {	str += r2Type; str += " *lwbOut2;\n\t"; }
	        str += r2Type; str += " *lwbOut;\n";
	      }
	      else if(outReal)
	      {
	        if(!rcSimple) {	str += rType; str += " *lwbOut2;\n\t"; }
	        str += rType; str += " *lwbOut;\n";
	      }
	      else
	      {
	        if(!rcSimple) {	str += rType; str += " *lwbOutRe2;\n\t"; }
	        if(!rcSimple) {	str += rType; str += " *lwbOutIm2;\n\t"; }
	        str += rType; str += " *lwbOutRe;\n\t";
	        str += rType; str += " *lwbOutIm;\n";
	      }
	      str += "\n";
      }
     else {
      if(params.fft_placeness == HCFFT_INPLACE) {
        str += "unsigned int ioOffset;\n\t";

      if(inInterleaved)
	    {
	      str += r2Type; str += " *lwb;\n";
	    }
	    else
	    {
	      str += rType; str += " *lwbRe;\n\t";
	      str += rType; str += " *lwbIm;\n";
	    }
    }
    else {
      str += "unsigned int iOffset;\n\t";
      str += "unsigned int oOffset;\n\t";

	    if(inInterleaved)
	    {
	      str += r2Type; str += " *lwbIn;\n\t";
	    }
	    else
	    {
	      str += rType; str += " *lwbInRe;\n\t";
	      str += rType; str += " *lwbInIm;\n\t";
	    }

	    if(outInterleaved)
	    {
	      str += r2Type; str += " *lwbOut;\n";
	    }
	    else
	    {
	      str += rType; str += " *lwbOutRe;\n\t";
	      str += rType; str += " *lwbOutIm;\n";
	    }
	  str += "\n";
	  }
   }

      // Setup registers if needed
      if(linearRegs) {
        str += "\t";
        str += RegBaseType<PR>(2);
        str += " ";
        str += IterRegs("", false);
        str += ";\n\n";
      }

      // Calculate total transform count
      std::string totalBatch = "(";
      size_t i = 0;

      while(i < (params.fft_DataDim - 2)) {
        totalBatch += SztToStr(params.fft_N[i + 1]);
        totalBatch += " * ";
        i++;
      }

      totalBatch += "batchSize)";

      // Conditional read-write ('rw') for arbitrary batch number
      if(r2c2r && !rcSimple) {
        str += "\tunsigned int thisvar = ";
        str += totalBatch;
        str += " - batch*";
        str +=  SztToStr(2 * numTrans);
        str += ";\n";
        str += "\tunsigned int rw = (me < ((thisvar+1)/2)*";
        str += SztToStr(workGroupSizePerTrans);
        str += ") ? (thisvar - 2*(me/";
        str += SztToStr(workGroupSizePerTrans);
        str += ")) : 0;\n\n";
      } else {
        if( (numTrans > 1) && !blockCompute ) {
          str += "\tunsigned int rw = (me < (";
          str += totalBatch;
          str += " - batch*";
          str += SztToStr(numTrans);
          str += ")*";
          str += SztToStr(workGroupSizePerTrans);
          str += ") ? 1 : 0;\n\n";
        } else {
          str += "\tunsigned int rw = 1;\n\n";
        }
      }

      // Transform index for 3-step twiddles
      if(params.fft_3StepTwiddle && !blockCompute) {
        if(numTrans == 1) {
          str += "\tunsigned int b = batch%";
        } else {
          str += "\tunsigned int b = (batch*";
          str += SztToStr(numTrans);
          str += " + (me/";
          str += SztToStr(workGroupSizePerTrans);
          str += "))%";
        }

        str += SztToStr(params.fft_N[1]);
        str += ";\n\n";

        if(params.fft_realSpecial) {
          str += "\tunsigned int bt = b;\n\n";
        }
      } else {
        str += "\tunsigned int b = 0;\n\n";
      }

      // Setup memory pointers
      if(r2c2r) {
        str += OffsetCalc("iOffset", true);
        str += OffsetCalc("oOffset", false);

        if(!rcSimple) {
          str += OffsetCalc("iOffset2",  true, true);
        }

        if(!rcSimple) {
          str += OffsetCalc("oOffset2", false, true);
        }

	      if(params.fft_placeness == HCFFT_INPLACE)
	      {
		      if(inInterleaved)
		      {
			      if(!rcSimple) {	str += "lwbIn2 = ( "; str += r2Type; str += " *)gb + iOffset2;\n\t"; }
			      str += "lwbIn  = ( "; str += r2Type; str += " *)gb + iOffset;\n\t";
		      }
		      else
		      {
			      if(!rcSimple) {	str += "lwbIn2 = ( "; str += rType; str += " *)gb + iOffset2;\n\t"; }
			      str += "lwbIn  = ( "; str += rType; str += " *)gb + iOffset;\n\t";

		      }
		      if(!rcSimple) {	str += "lwbOut2 = gb + oOffset2;\n\t"; }
		      str += "lwbOut = gb + oOffset;\n";
		      str += "\n";
	      }
	      else
	      {
          if(inInterleaved || inReal)
			    {
				    if(!rcSimple) {	str += "lwbIn2 = gbIn + iOffset2;\n\t"; }
				    str += "lwbIn = gbIn + iOffset;\n\t";
			    }
			    else
		      {
			      if(!rcSimple) {	str += "lwbInRe2 = gbInRe + iOffset2;\n\t"; }
			      if(!rcSimple) {	str += "lwbInIm2 = gbInIm + iOffset2;\n\t"; }
					  str += "lwbInRe = gbInRe + iOffset;\n\t";
					  str += "lwbInIm = gbInIm + iOffset;\n\t";
		      }

          if(outInterleaved || outReal)
	        {
	          if(!rcSimple) {	str += "lwbOut2 = gbOut + oOffset2;\n\t"; }
	          str += "lwbOut = gbOut + oOffset;\n";
	        }
	        else
	        {
		        if(!rcSimple) {	str += "lwbOutRe2 = gbOutRe + oOffset2;\n\t"; }
		        if(!rcSimple) {	str += "lwbOutIm2 = gbOutIm + oOffset2;\n\t"; }
						str += "lwbOutRe = gbOutRe + oOffset;\n\t";
						str += "lwbOutIm = gbOutIm + oOffset;\n";
	        }
          str += "\n";
	    }
      }
      else {
        if(params.fft_placeness == HCFFT_INPLACE) {
          if(blockCompute) {
            str += OffsetCalcBlock("ioOffset", true);
          } else {
            str += OffsetCalc("ioOffset", true);
          }

          str += "\t";

					if(inInterleaved)
					{
						str += "lwb = gb + ioOffset;\n";
					}
					else
					{
						str += "lwbRe = gbRe + ioOffset;\n\t";
						str += "lwbIm = gbIm + ioOffset;\n";
					}
				str += "\n";
				}
        else {
          if(blockCompute) {
            str += OffsetCalcBlock("iOffset", true);
            str += OffsetCalcBlock("oOffset", false);
          } else {
            str += OffsetCalc("iOffset", true);
            str += OffsetCalc("oOffset", false);
          }

          str += "\t";

	if(inInterleaved)
	{
		str += "lwbIn = gbIn + iOffset;\n\t";
	}
	else
	{
		str += "lwbInRe = gbInRe + iOffset;\n\t";
		str += "lwbInIm = gbInIm + iOffset;\n\t";
	}

	  if(outInterleaved)
	  {
			  str += "lwbOut = gbOut + oOffset;\n";
	  }
	  else
	  {
		  str += "lwbOutRe = gbOutRe + oOffset;\n\t";
		  str += "lwbOutIm = gbOutIm + oOffset;\n";
	  }
	  str += "\n";
        }
      }

	std::string inOffset;
	std::string outOffset;
	if (params.fft_placeness == HCFFT_INPLACE && !r2c2r)
	{
		inOffset += "ioOffset";
		outOffset += "ioOffset";
	}
	else
	{
		inOffset += "iOffset";
		outOffset += "oOffset";
	}

      // Read data into LDS for blocked access
      if(blockCompute) {
        size_t loopCount = (length * blockWidth) / blockWGS;
        str += "\n\tfor(uint t=0; t<";
        str += SztToStr(loopCount);
        str += "; t++)\n\t{\n";

				//get offset 
				std::string bufOffset;

        for(size_t c = 0; c < 2; c++) {
          std::string comp = "";
          std::string readBuf = (params.fft_placeness == HCFFT_INPLACE) ? "lwb" : "lwbIn";

          if(!inInterleaved) {
            comp = c ? ".y" : ".x";
          }

          if(!inInterleaved) {
            readBuf = (params.fft_placeness == HCFFT_INPLACE) ? (c ? "lwbIm" : "lwbImRe") : (c ? "lwbIm" : "lwbImRe");
          }

	if( (blockComputeType == BCT_C2C) || (blockComputeType == BCT_C2R) )
	{
		bufOffset.clear();
		bufOffset += "(me%"; bufOffset += SztToStr(blockWidth); bufOffset += ") + ";
		bufOffset += "(me/"; bufOffset+= SztToStr(blockWidth); bufOffset+= ")*"; bufOffset += SztToStr(params.fft_inStride[0]);
		bufOffset += " + t*"; bufOffset += SztToStr(params.fft_inStride[0]*blockWGS/blockWidth);

	str += "\t\tR0"; str+= comp; str+= " = "; 
		str += readBuf; str += "[";	str += bufOffset; str += "];\n";
	}
	else
	{
		str += "\t\tR0"; str+= comp; str+= " = "; str += readBuf; str += "[me + t*"; str += SztToStr(blockWGS); str += "];\n";
	}

          if(inInterleaved) {
            break;
          }
        }

	if( (blockComputeType == BCT_C2C) || (blockComputeType == BCT_C2R) )
	{
		str += "\t\tlds[t*"; str += SztToStr(blockWGS/blockWidth); str += " + ";
		str += "(me%"; str+= SztToStr(blockWidth); str+= ")*"; str += SztToStr(length); str += " + ";
		str += "(me/"; str+= SztToStr(blockWidth); str+= ")] = R0;"; str +="\n";
	}
	else
	{
		str += "\t\tlds[t*"; str += SztToStr(blockWGS); str += " + me] = R0;"; str +="\n";
	}

        str += "\t}\n\n";
        str += "\t tidx.barrier.wait_with_tile_static_memory_fence();\n\n";
      }

      // Set rw and 'me' per transform
      // rw string also contains 'b'
      std::string rw, me;

      if(r2c2r && !rcSimple) {
        rw = "rw, b, ";
      } else {
        rw = ((numTrans > 1) || realSpecial) ? "rw, b, " : "1, b, ";
      }

      if(numTrans > 1)  {
        me += "me%";
        me += SztToStr(workGroupSizePerTrans);
        me += ", ";
      } else        {
        me += "me, ";
      }

	if(blockCompute) { me = "me%"; me += SztToStr(workGroupSizePerTrans); me += ", "; }

      // Buffer strings
      std::string inBuf, outBuf;

      if(r2c2r) {
        if(rcSimple) {
          if(inInterleaved || inReal) {
            inBuf  = "lwbIn, ";
          } else {
            inBuf  = "lwbInRe, lwbInIm,";
          }

          if(outInterleaved || outReal) {
            outBuf = "lwbOut";
          } else {
            outBuf = "lwbOutRe, lwbOutIm";
          }
        } else {
          if(inInterleaved || inReal) {
            inBuf  = "lwbIn, lwbIn2, ";
          } else {
            inBuf  = "lwbInRe, lwbInRe2, lwbInIm, lwbInIm2, ";
          }

          if(outInterleaved || outReal) {
            outBuf = "lwbOut, lwbOut2";
          } else {
            outBuf = "lwbOutRe, lwbOutRe2, lwbOutIm, lwbOutIm2";
          }
        }
      } else {
        if(params.fft_placeness == HCFFT_INPLACE) {
          if(inInterleaved) {
            inBuf = "lwb, ";
            outBuf = "lwb";
          } else        {
            inBuf = "lwbRe, lwbIm,";
            outBuf = "lwbRe, lwbIm";
          }
        } else {
          if(inInterleaved) {
            inBuf  = "lwbIn, ";
          } else {
            inBuf  = "lwbInRe, lwbInIm, ";
          }

          if(outInterleaved) {
            outBuf = "lwbOut";
          } else {
            outBuf = "lwbOutRe, lwbOutIm";
          }
        }
      }

      if(blockCompute) {
        str += "\n\tfor(uint t=0; t<";
        str += SztToStr(blockWidth / (blockWGS / workGroupSizePerTrans));
        str += "; t++)\n\t{\n\n";
        inBuf = "lds, ";
        outBuf = "lds";

        if(params.fft_3StepTwiddle) {
          str += "\t\tb = (batch%";
          str += SztToStr(params.fft_N[1] / blockWidth);
          str += ")*";
          str += SztToStr(blockWidth);
          str += " + t*";
          str += SztToStr(blockWGS / workGroupSizePerTrans);
          str += " + (me/";
          str += SztToStr(workGroupSizePerTrans);
          str += ");\n\n";
        }
      }

      if(realSpecial) {
        str += "\n\tfor(uint t=0; t<2; t++)\n\t{\n\n";
      }

      // Call passes
      if(numPasses == 1) {
        str += "\t";
        str += PassName(count, 0, fwd);
        str += "(";
        str += rw;
        str += me;
        str += "0, 0, ";
        str += inBuf;
        str += outBuf;
        str += IterRegs("&");

        if(length > 1) {
          str += ",";
          str += TwTableName();
        }

        if(params.fft_3StepTwiddle) {
          str += ",";
          str += TwTableLargeName();
        }

        str += ",tidx);\n";
      } else {
        for(typename std::vector<Pass<PR> >::const_iterator p = passes.begin(); p != passes.end(); p++) {
          bool tw3Step = false;

          if(p == passes.begin() && params.fft_twiddleFront ) {
            tw3Step = params.fft_3StepTwiddle;
          }

          if((p + 1) == passes.end()) {
            if(!params.fft_twiddleFront) {
              tw3Step = params.fft_3StepTwiddle;
            }
          }

          std::string exTab = "";

          if(blockCompute || realSpecial) {
            exTab = "\t";
          }

          str += exTab;
          str += "\t";
          str += PassName(count, p->GetPosition(), fwd);
          str += "(";
          std::string ldsOff;

          if(blockCompute) {
            ldsOff += "t*";
            ldsOff += SztToStr(length * (blockWGS / workGroupSizePerTrans));
            ldsOff += " + (me/";
            ldsOff += SztToStr(workGroupSizePerTrans);
            ldsOff += ")*";
            ldsOff += SztToStr(length);
          } else {
            if(numTrans > 1) {
              ldsOff += "(me/";
              ldsOff += SztToStr(workGroupSizePerTrans);
              ldsOff += ")*";
              ldsOff += SztToStr(length);
            } else {
              ldsOff += "0";
            }
          }

          std::string ldsArgs;

          if(halfLds) {
            ldsArgs += "lds, lds";
          } else    {
            if(ldsInterleaved) {
              ldsArgs += "lds";
            } else {
              ldsArgs += "lds, lds + ";
              ldsArgs += SztToStr(length * numTrans);
            }
          }

          str += rw;

          if(params.fft_realSpecial) {
            str += "t, ";
          }

          str += me;

          if(p == passes.begin()) { // beginning pass
            str += blockCompute ? ldsOff : "0";
            str += ", ";
            str += ldsOff;
            str += ", ";
            str += inBuf;
            str += ldsArgs;
            str += IterRegs("&");

            if(length > 1) {
              str += ",";
              str += TwTableName();
            }

            if(tw3Step) {
              str += ",";
              str += TwTableLargeName();
            }

            str += ",tidx);\n";

            if(!halfLds) {
              str += exTab;
              str += "\t tidx.barrier.wait_with_tile_static_memory_fence();\n";
            }
          } else if((p + 1) == passes.end()) { // ending pass
            str += ldsOff;
            str += ", ";
            str += blockCompute ? ldsOff : "0";
            str += ", ";
            str += ldsArgs;
            str += ", ";
            str += outBuf;
            str += IterRegs("&");

            if(length > 1) {
              str += ",";
              str += TwTableName();
            }

            if(tw3Step) {
              str += ",";
              str += TwTableLargeName();
            }

            str += ",tidx);\n";
            if(!halfLds) { str += exTab; str += "\ttidx.barrier.wait_with_tile_static_memory_fence();\n"; }

          } else { // intermediate pass
            str += ldsOff;
            str += ", ";
            str += ldsOff;
            str += ", ";
            str += ldsArgs;
            str += ", ";
            str += ldsArgs;
            str += IterRegs("&");

            if(length > 1) {
              str += ",";
              str += TwTableName();
            }

            if(tw3Step) {
              str += ",";
              str += TwTableLargeName();
            }

            str += ",tidx);\n";

            if(!halfLds) {
              str += exTab;
              str += "\t tidx.barrier.wait_with_tile_static_memory_fence();\n";
            }
          }
        }
      }

      if(realSpecial) {
        size_t Nt = 1 + length / 2;
        str +=  "\n\t\tif( (bt == 0) || (2*bt == ";
        str += SztToStr(params.fft_realSpecial_Nr);
        str += ") ) break;\n";
        str += "\t\tgbOut += (";
        str += SztToStr(params.fft_realSpecial_Nr);
        str += " - 2*bt)*";
        str += SztToStr(Nt);
        str += ";\n";
        str += "\t\tb = ";
        str += SztToStr(params.fft_realSpecial_Nr);
        str += " - b;\n\n";
      }

      if(blockCompute || realSpecial) {
        str += "\n\t}\n\n";
      }

      // Write data from LDS for blocked access
      if(blockCompute) {
        size_t loopCount = (length * blockWidth) / blockWGS;
        str += "\t tidx.barrier.wait_with_tile_static_memory_fence();\n\n";
        str += "\n\tfor(uint t=0; t<";
        str += SztToStr(loopCount);
        str += "; t++)\n\t{\n";

					if( (blockComputeType == BCT_C2C) || (blockComputeType == BCT_R2C) ) {
          str += "\t\tR0 = lds[t*";
          str += SztToStr(blockWGS / blockWidth);
          str += " + ";
          str += "(me%";
          str += SztToStr(blockWidth);
          str += ")*";
          str += SztToStr(length);
          str += " + ";
          str += "(me/";
          str += SztToStr(blockWidth);
          str += ")];";
          str += "\n";
        } else {
          str += "\t\tR0 = lds[t*";
          str += SztToStr(blockWGS);
          str += " + me];";
          str += "\n";
        }

        for(size_t c = 0; c < 2; c++) {
          std::string comp = "";
          std::string writeBuf = (params.fft_placeness == HCFFT_INPLACE) ? "lwb" : "lwbOut";

          if(!outInterleaved) {
            comp = c ? ".y" : ".x";
          }

          if(!outInterleaved) {
            writeBuf = (params.fft_placeness == HCFFT_INPLACE) ? (c ? "lwbIm" : "lwbRe") : (c ? "lwbOutIm" : "lwbOutRe");
          }

					if( (blockComputeType == BCT_C2C) || (blockComputeType == BCT_R2C) ) {
            str += "\t\t";
            str += writeBuf;
            str += "[(me%";
            str += SztToStr(blockWidth);
            str += ") + ";
            str += "(me/";
            str += SztToStr(blockWidth);
            str += ")*";
            str += SztToStr(params.fft_outStride[0]);
            str += " + t*";
            str += SztToStr(params.fft_outStride[0] * blockWGS / blockWidth);
            str += "] = R0";
            str += comp;
            str += ";\n";
          } else {
            str += "\t\t";
            str += writeBuf;
            str += "[me + t*";
            str += SztToStr(blockWGS);
            str += "] = R0";
            str += comp;
            str += ";\n";
          }

          if(outInterleaved) {
            break;
          }
        }

        str += "\t}\n\n";
      }

      str += " }).wait();\n";

      str += "}}\n\n";

      if(r2c2r) {
        break;
      }
    }
  }

};
}

using namespace StockhamGenerator;

template<>
hcfftStatus FFTPlan::GetMax1DLengthPvt<Stockham> (size_t* longest) const {
  // TODO  The caller has already acquired the lock on *this
  //  However, we shouldn't depend on it.
  //  Query the devices in this context for their local memory sizes
  //  How large a kernel we can generate depends on the *minimum* LDS
  //  size for all devices.
  //
  const FFTEnvelope* pEnvelope = NULL;
  this->GetEnvelope (& pEnvelope);
  BUG_CHECK (NULL != pEnvelope);
  ARG_CHECK (NULL != longest)
  size_t LdsperElement = this->ElementSize();
  size_t result = pEnvelope->limit_LocalMemSize /
                  (1 * LdsperElement);
  result = FloorPo2 (result);
  *longest = result;
  return HCFFT_SUCCEEDS;
}


template<>
hcfftStatus FFTPlan::GetKernelGenKeyPvt<Stockham> (FFTKernelGenKeyParams & params) const {
  //    Query the devices in this context for their local memory sizes
  //    How we generate a kernel depends on the *minimum* LDS size for all devices.
  //
  const FFTEnvelope* pEnvelope = NULL;
  const_cast<FFTPlan*>(this)->GetEnvelope (& pEnvelope);
  BUG_CHECK (NULL != pEnvelope);
  ::memset( &params, 0, sizeof( params ) );
  params.fft_precision    = this->precision;
  params.fft_placeness    = this->location;
  params.fft_inputLayout  = this->ipLayout;
  params.fft_MaxWorkGroupSize = this->envelope.limit_WorkGroupSize;
  ARG_CHECK (this->inStride.size() == this->outStride.size())
  bool real_transform = ((this->ipLayout == HCFFT_REAL) || (this->opLayout == HCFFT_REAL));

  if ( (HCFFT_INPLACE == this->location) && (!real_transform) ) {
    //    If this is an in-place transform the
    //    input and output layout, dimensions and strides
    //    *MUST* be the same.
    //
    ARG_CHECK (this->ipLayout == this->opLayout)
    params.fft_outputLayout = this->ipLayout;

    for (size_t u = this->inStride.size(); u-- > 0; ) {
      ARG_CHECK (this->inStride[u] == this->outStride[u]);
    }
  } else {
    params.fft_outputLayout = this->opLayout;
  }

  params.fft_DataDim = this->length.size() + 1;
  int i = 0;

  for(i = 0; i < (params.fft_DataDim - 1); i++) {
    params.fft_N[i]         = this->length[i];
    params.fft_inStride[i]  = this->inStride[i];
    params.fft_outStride[i] = this->outStride[i];
  }

  params.fft_inStride[i]  = this->iDist;
  params.fft_outStride[i] = this->oDist;
  params.fft_RCsimple = this->RCsimple;
  params.fft_realSpecial = this->realSpecial;
  params.fft_realSpecial_Nr = this->realSpecial_Nr;
  params.blockCompute = this->blockCompute;
  params.blockComputeType = this->blockComputeType;
  params.fft_twiddleFront = this->twiddleFront;
  size_t wgs, nt;
  size_t t_wgs, t_nt;
  Precision pr = (params.fft_precision == HCFFT_SINGLE) ? P_SINGLE : P_DOUBLE;

  switch(pr) {
    case P_SINGLE: {
        KernelCoreSpecs<P_SINGLE> kcs;
        kcs.GetWGSAndNT(params.fft_N[0], t_wgs, t_nt);

        if(params.blockCompute) {
          params.blockSIMD = Kernel<P_SINGLE>::BlockSizes::BlockWorkGroupSize(params.fft_N[0]);
          params.blockLDS  = Kernel<P_SINGLE>::BlockSizes::BlockLdsSize(params.fft_N[0]);
        }
      }
      break;

    case P_DOUBLE: {
        KernelCoreSpecs<P_DOUBLE> kcs;
        kcs.GetWGSAndNT(params.fft_N[0], t_wgs, t_nt);

        if(params.blockCompute) {
          params.blockSIMD = Kernel<P_DOUBLE>::BlockSizes::BlockWorkGroupSize(params.fft_N[0]);
          params.blockLDS  = Kernel<P_DOUBLE>::BlockSizes::BlockLdsSize(params.fft_N[0]);
        }
      }
      break;
  }

  if((t_wgs != 0) && (t_nt != 0) && (this->envelope.limit_WorkGroupSize >= 256)) {
    wgs = t_wgs;
    nt = t_nt;
  } else {
    DetermineSizes(this->envelope.limit_WorkGroupSize, params.fft_N[0], wgs, nt, pr);
  }

  assert((nt * params.fft_N[0]) >= wgs);
  assert((nt * params.fft_N[0]) % wgs == 0);
  params.fft_R = (nt * params.fft_N[0]) / wgs;
  params.fft_SIMD = wgs;

  if (this->large1D != 0) {
    ARG_CHECK (params.fft_N[0] != 0)
    ARG_CHECK ((this->large1D % params.fft_N[0]) == 0)
    params.fft_3StepTwiddle = true;

    if(!(this->realSpecial)) {
      ARG_CHECK ( this->large1D  == (params.fft_N[1] * params.fft_N[0]) );
    }
  }

  params.fft_fwdScale  = this->forwardScale;
  params.fft_backScale = this->backwardScale;
  return HCFFT_SUCCEEDS;
}

template<>
hcfftStatus FFTPlan::GetWorkSizesPvt<Stockham> (std::vector<size_t> & globalWS, std::vector<size_t> & localWS) const {
  //    How many complex numbers in the input mutl-dimensional array?
  //
  unsigned long long count = 1;

  for (unsigned u = 0; u < length.size(); ++u) {
    count *= std::max<size_t> (1, this->length[ u ]);
  }

  count *= this->batchSize;
  FFTKernelGenKeyParams fftParams;
  //    Translate the user plan into the structure that we use to map plans to clPrograms
  this->GetKernelGenKeyPvt<Stockham>( fftParams );

  if(fftParams.blockCompute) {
    count = DivRoundingUp<unsigned long long> (count, fftParams.blockLDS);
    count = count * fftParams.blockSIMD;
    globalWS.push_back( static_cast< size_t >( count ) );
    localWS.push_back( fftParams.blockSIMD );
    return    HCFFT_SUCCEEDS;
  }

  count = DivRoundingUp<unsigned long long> (count, fftParams.fft_R);      // count of WorkItems
  count = DivRoundingUp<unsigned long long> (count, fftParams.fft_SIMD);   // count of WorkGroups

  // for real transforms we only need half the work groups since we do twice the work in 1 work group
  if( !(fftParams.fft_RCsimple) && ((fftParams.fft_inputLayout == HCFFT_REAL) || (fftParams.fft_outputLayout == HCFFT_REAL)) ) {
    count = DivRoundingUp<unsigned long long> (count, 2);
  }

  count = std::max<unsigned long long> (count, 1) * fftParams.fft_SIMD;
  // .. count of WorkItems, rounded up to next multiple of fft_SIMD.
  // 1 dimension work group size
  globalWS.push_back( static_cast< size_t >( count ) );
  localWS.push_back( fftParams.fft_SIMD );
  return    HCFFT_SUCCEEDS;
}

template<>
hcfftStatus FFTPlan::GenerateKernelPvt<Stockham>(const hcfftPlanHandle plHandle, FFTRepo& fftRepo, size_t count, bool exist) const {
  FFTKernelGenKeyParams params;
  this->GetKernelGenKeyPvt<Stockham> (params);
  if(!exist)
  {
    vector< size_t > gWorkSize;
    vector< size_t > lWorkSize;
    this->GetWorkSizesPvt<Stockham> (gWorkSize, lWorkSize);
    std::string programCode;
    programCode = hcHeader();
    Precision pr = (params.fft_precision == HCFFT_SINGLE) ? P_SINGLE : P_DOUBLE;

    switch(pr) {
      case P_SINGLE: {
          Kernel<P_SINGLE> kernel(params);
          kernel.GenerateKernel((void**)&twiddles, (void**)&twiddleslarge, acc, plHandle, programCode, gWorkSize, lWorkSize, count);
        }
        break;

      case P_DOUBLE: {
          Kernel<P_DOUBLE> kernel(params);
          kernel.GenerateKernel((void**)&twiddles, (void**)&twiddleslarge, acc, plHandle, programCode, gWorkSize, lWorkSize, count);
        }
        break;
     }

    fftRepo.setProgramCode( Stockham, plHandle, params, programCode);
    fftRepo.setProgramEntryPoints( Stockham, plHandle, params, "fft_fwd", "fft_back");
  }
  else
  {
    size_t large1D = 0;
    size_t length = params.fft_N[0];
    size_t R = length;
    const size_t* pRadices = NULL;
    size_t nPasses;

    if(params.fft_realSpecial) {
      large1D = params.fft_N[0] * params.fft_realSpecial_Nr;
    } else {
      large1D = params.fft_N[0] * params.fft_N[1];
    }

    std::vector<size_t> radices;
    KernelCoreSpecs<P_SINGLE> kcs;
    kcs.GetRadices(length, nPasses, pRadices);

    if((params.fft_MaxWorkGroupSize >= 256) && (pRadices != NULL))
    {
      for(size_t i = 0; i < nPasses; i++) {
        size_t rad = pRadices[i];
        R /= rad;
        radices.push_back(rad);
      }

      assert(R == 1); // this has to be true for correct radix composition of the length
    }
    else
    {
      size_t numTrans = (params.fft_SIMD * params.fft_R) / length;
      size_t cnPerWI = (numTrans * length) / params.fft_SIMD;

      // Possible radices
      size_t cRad[] = {13, 11, 10, 8, 7, 6, 5, 4, 3, 2, 1}; // Must be in descending order
      size_t cRadSize = (sizeof(cRad) / sizeof(cRad[0]));

      while(true) {
        size_t rad;
        assert(cRadSize >= 1);

        for(size_t r = 0; r < cRadSize; r++) {
          rad = cRad[r];

          if((rad > cnPerWI) || (cnPerWI % rad)) {
            continue;
          }

          if(!(R % rad)) {
            break;
          }
        }

        assert((cnPerWI % rad) == 0);
        R /= rad;
        radices.push_back(rad);
        assert(R >= 1);

        if(R == 1) {
          break;
        }
      }
    }

    if(params.fft_precision == HCFFT_SINGLE)
    {
      // Twiddle table
      if(length > 1) {
        TwiddleTable<hc::short_vector::float_2> twTable(length);
        twTable.GenerateTwiddleTable((void**)&twiddles, acc, radices);
      }

      // twiddle factors for 1d-large 3-step algorithm
      if(params.fft_3StepTwiddle && !twiddleslarge) {
        TwiddleTableLarge<hc::short_vector::float_2, P_SINGLE> twLarge(large1D);
        twLarge.TwiddleLargeAV((void**)&twiddleslarge, acc);
      }
    }
    else
    {
      // Twiddle table
      if(length > 1) {
        TwiddleTable<hc::short_vector::double_2> twTable(length);
        twTable.GenerateTwiddleTable((void**)&twiddles, acc, radices);
      }

      // twiddle factors for 1d-large 3-step algorithm
      if(params.fft_3StepTwiddle && !twiddleslarge) {
        TwiddleTableLarge<hc::short_vector::double_2, P_DOUBLE> twLarge(large1D);
        twLarge.TwiddleLargeAV((void**)&twiddleslarge, acc);
      }
    }
  }

  return HCFFT_SUCCEEDS;
}
