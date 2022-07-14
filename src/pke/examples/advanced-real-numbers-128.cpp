//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
  Advanced examples for 128-bit implementation of CKKS
 */

 // Define PROFILE to enable TIC-TOC timing measurements
#define PROFILE

#include "openfhe.h"
#include "scheme/ckksrns/cryptocontext-ckksrns.h"
#include "gen-cryptocontext.h"

using namespace lbcrypto;

void AutomaticRescaleDemo(RescalingTechnique rsTech);
void ManualRescaleDemo(RescalingTechnique rsTech);
void HybridKeySwitchingDemo1();
void HybridKeySwitchingDemo2();
void FastRotationsDemo1();
void FastRotationsDemo2();

int main(int argc, char* argv[]) {
  /*
   * Our 128-bit implementation of CKKS includes two variants called
   * "FIXEDMANUAL" and "FIXEDAUTO", respectively.  Note that 128-bit
   * CKKS supports does not support the FLEXIBLEAUTO mode.
   *
   * To turn on the 128-bit mode, run "cmake -DNATIVE_SIZE=128 .."
   *
   * Before we start, we need to say a few words about the rescale
   * operation, which is central in CKKS. Whenever we multiply two
   * ciphertexts c1 and c2 which encrypt numbers m1*D and m2*D
   * respectively, we get a result that looks like m1*m2*D^2. Since the
   * scaling factor of this number is D^2, we say that the result is of
   * depth 2. It is clear that a ciphertext of depth 2 cannot be added
   * to ciphertexts of depth 1, because their scaling factors are
   * different. Rescaling takes a ciphertext of depth 2, and makes it of
   * depth 1 by an operation that looks a lot like dividing by D=2^p.
   *
   * For efficiency reasons, our implementation of CKKS works in the
   * RNS space, which means that we avoid working with big numbers and
   * we only work with native integers. One complication that arises
   * from this is that we can only rescale by dividing by certain prime
   * numbers and not D=2^p.
   *
   * There are two ways to deal with this. The first is to choose prime
   * numbers as close to 2^p as possible, and assume that the scaling
   * factor remains the same. This inevitably incurs some approximation
   * error, and this is why we refer to it as the FIXEDMANUAL variant.
   * The second way of dealing with this is to track how the scaling
   * factor changes and try to adjust for it. This is what we call the
   * FLEXIBLEAUTO variant of CKKS. Only the approximate approach is supported
   * for 128-bit CKKS. We also include FIXEDAUTO, which is an automated
   * version of FIXEDMANUAL that does all rescaling automatically.
   *
   * We have designed FIXEDAUTO so it hides all the nuances of
   * tracking the depth of ciphertexts and having to call the rescale
   * operation. Therefore, FIXEDAUTO is more appropriate for users
   * who do not want to get into the details of the underlying crypto
   * and math, or who want to put together a quick prototype. On the
   * contrary, FIXEDMANUAL is more appropriate for production
   * applications that have been optimized by experts.
   *
   * The first two parts of this demo introduce the two variants, by
   * implementing the same computation, using both FIXEDAUTO and FIXEDMANUAL.
   *
   */

#if NATIVEINT == 128
  AutomaticRescaleDemo(FIXEDAUTO);
  // Note that FLEXIBLEAUTO is not supported for 128-bit CKKS

  ManualRescaleDemo(FIXEDMANUAL);

  /*
   * Our implementation of CKKS supports three different algorithms
   * for key switching, namely BV, GHS, and HYBRID. BV corresponds to
   * a technique also known as digit decomposition (both RNS and based
   * on a digit size). GHS corresponds to ciphertext
   * modulus doubling, and HYBRID combines the characteristics of both
   * BV and GHS. Please refer to the documentation of KeySwitchBVGen,
   * KeySwitchGHSGen, and KeySwitchHybridGen in scheme/ckks/ckks.h for more
   * details about the different key switch techniques.
   *
   * For most cases, HYBRID will be the most appropriate and efficient
   * key switching technique, and this is why we devote the third and
   * fourth part of this demo to HYBRID key switching.
   */
  HybridKeySwitchingDemo1();
  HybridKeySwitchingDemo2();

  /*
   * The final parts of this demo showcase our implementation of an
   * optimization technique called hoisting. The idea is simple - when
   * we want to perform multiple different rotations to the same
   * ciphertext, we can compute one part of the rotation algorithm once,
   * and reuse it multiple times. Please refer to the documentation of
   * EvalFastRotationPrecomputeBV, EvalFastRotationPrecomputeGHS, and
   * EvalFastRotationPrecomputeHybrid in scheme/ckks/ckks.h for more details on
   * hoisting in BV, GHS, and HYBRID key switching.
   */
  FastRotationsDemo1();
  FastRotationsDemo2();
#else
  std::cout << "This demo only runs for 128-bit CKKS." << std::endl;
#endif

  return 0;
}

void AutomaticRescaleDemo(RescalingTechnique rsTech) {
  /* Please read comments in main() for an introduction to what the
   * rescale operation is. Knowing about Rescale() is not necessary
   * to use the FIXEDAUTO CKKS variant, it is however needed to
   * understand what's happening underneath.
   *
   * FIXEDAUTO is a variant of CKKS that automatically
   *    performs rescaling before every multiplication.
   *    This is done to make it easier for users to write FHE
   *    computations without worrying about the depth of ciphertexts
   *    or rescaling.
   */
  if (rsTech == FIXEDAUTO) {
    std::cout << "\n\n\n ===== ApproxAutoDemo ============= " << std::endl;
  }

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(6);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

  std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension()
            << std::endl
            << std::endl;

  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  auto keys = cc->KeyGen();
  cc->EvalMultKeyGen(keys.secretKey);

  // Input
  std::vector<double> x = {1.0, 1.01, 1.02, 1.03, 1.04, 1.05, 1.06, 1.07};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  /* Computing f(x) = c*x^18 + c*x^9 + d
   *
   * In the following we compute f(x) with a computation
   * that has a multiplicative depth of 5 or 6.
   *
   * The result is correct, even though there is no call to
   * the Rescale() operation.
   */
  auto c2 = cc->EvalMult(c, c);                          // x^2
  auto c4 = cc->EvalMult(c2, c2);                        // x^4
  auto c8 = cc->EvalMult(c4, c4);                        // x^8
  auto c16 = cc->EvalMult(c8, c8);                       // x^16
  auto c9 = cc->EvalMult(c8, c);                         // x^9
  auto c18 = cc->EvalMult(c16, c2);                      // x^18
  auto cRes1 = cc->EvalAdd(cc->EvalAdd(c18, c9), 1.0);   // Final result 1
  auto cRes2 = cc->EvalSub(cc->EvalAdd(c18, c9), 1.0);   // Final result 2
  auto cRes3 = cc->EvalMult(cc->EvalAdd(c18, c9), 0.5);  // Final result 3

  Plaintext result1, result2, result3;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cRes1, &result1);
  result1->SetLength(batchSize);
  std::cout << "x^18 + x^9 + 1 = " << result1 << std::endl;

  cc->Decrypt(keys.secretKey, cRes2, &result2);
  result2->SetLength(batchSize);
  std::cout << "x^18 + x^9 - 1 = " << result2 << std::endl;

  cc->Decrypt(keys.secretKey, cRes3, &result3);
  result3->SetLength(batchSize);
  std::cout << "0.5*x^18 + 0.5*x^9 = " << result3 << std::endl;
}

void ManualRescaleDemo(RescalingTechnique rsTech) {
  /* Please read comments in main() for an introduction to what the
   * rescale operation is, and what's the FIXEDMANUAL variant of CKKS.
   *
   * Even though FIXEDMANUAL does not implement automatic rescaling
   * as FIXEDAUTO does, this does not mean that it does not abstract
   * away some of the nitty-gritty details of using CKKS.
   *
   * In CKKS, ciphertexts are defined versus a large ciphertext modulus Q.
   * Whenever we rescale a ciphertext, its ciphertext modulus becomes
   * smaller too. All homomorphic operations require that their inputs are
   * defined over the same ciphertext modulus, and therefore, we need to
   * adjust one of them if their ciphertext moduli do not match. The way
   * this is done in the original CKKS paper is through an operation called
   * Modulus Switch. In our implementation, we call this operation
   * LevelReduce, and both FIXEDMANUAL and FIXEDAUTO do it automatically.
   * As far as we know, automatic level reduce does not incur any performance
   * penalty and this is why it is performed in both FIXEDMANUAL and
   * FIXEDAUTO.
   *
   * Overall, we believe that automatic modulus switching and rescaling make
   * CKKS much easier to use, at least for non-expert users.
   */
  std::cout << "\n\n\n ===== ApproxRescaleDemo ============= " << std::endl;

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(5);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension()
      << std::endl
      << std::endl;

  auto keys = cc->KeyGen();
  cc->EvalMultKeyGen(keys.secretKey);

  // Input
  std::vector<double> x = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  /* Computing f(x) = x^18 + x^9 + 1
   *
   * Compare the following with the corresponding code
   * for FIXEDAUTO. Here we need to track the depth of ciphertexts
   * and call Rescale() whenever needed. In this instance it's still
   * not hard to do so, but this can be quite tedious in other
   * complicated computations (e.g., in bootstrapping).
   *
   */
  // x^2
  auto c2_depth2 = cc->EvalMult(c, c);
  auto c2_depth1 = cc->Rescale(c2_depth2);
  // x^4
  auto c4_depth2 = cc->EvalMult(c2_depth1, c2_depth1);
  auto c4_depth1 = cc->Rescale(c4_depth2);
  // x^8
  auto c8_depth2 = cc->EvalMult(c4_depth1, c4_depth1);
  auto c8_depth1 = cc->Rescale(c8_depth2);
  // x^16
  auto c16_depth2 = cc->EvalMult(c8_depth1, c8_depth1);
  auto c16_depth1 = cc->Rescale(c16_depth2);
  // x^9
  auto c9_depth2 = cc->EvalMult(c8_depth1, c);
  // x^18
  auto c18_depth2 = cc->EvalMult(c16_depth1, c2_depth1);
  // Final result
  auto cRes_depth2 = cc->EvalAdd(cc->EvalAdd(c18_depth2, c9_depth2), 1.0);
  auto cRes_depth1 = cc->Rescale(cRes_depth2);

  Plaintext result;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cRes_depth1, &result);
  result->SetLength(batchSize);
  std::cout << "x^18 + x^9 + 1 = " << result << std::endl;
}

void HybridKeySwitchingDemo1() {
  /*
   * Please refer to comments in the demo-simple_real_number.cpp
   * for a brief introduction on what key switching is and to
   * find reference for HYBRID key switching.
   *
   * In this demo, we focus on how to choose the number of digits
   * in HYBRID key switching, and how that affects the usage and
   * efficiency of the CKKS scheme.
   *
   */

  std::cout << "\n\n\n ===== HybridKeySwitchingDemo1 ============= "
            << std::endl;

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(5);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);
  parameters.SetRescalingTechnique(FIXEDAUTO);
  // uint32_t ringDimension = 0;  // 0 means the library will choose it based on securityLevel
  /*
   * dnum is the number of large digits in HYBRID decomposition
   *
   * If not supplied (or value 0 is supplied), the default value is
   * set as follows:
   * - If multiplicative depth is > 3, then dnum = 3 digits are used.
   * - If multiplicative depth is 3, then dnum = 2 digits are used.
   * - If multiplicative depth is < 3, then dnum is set to be equal to
   * multDepth+1
   */

  uint32_t dnum = 2;
  /* To understand the effects of changing dnum, it is important to
   * understand how the ciphertext modulus size changes during key
   * switching.
   *
   * In our RNS implementation of CKKS, every ciphertext corresponds
   * to a large number (which is represented as small integers in RNS)
   * modulo a ciphertext modulus Q, which is defined as the product of
   * (multDepth+1) prime numbers: Q = q0 * q1 * ... * qL. Each qi is
   * selected to be close to the scaling factor D=2^p, hence the total
   * size of Q is approximately:
   *
   * sizeof(Q) = (multDepth+1)*scaleFactorBits.
   *
   * HYBRID key switching takes a number d that's defined modulo Q,
   * and performs 4 steps:
   * 1 - Digit decomposition:
   *     Split d into dnum digits - the size of each digit is roughly
   *     ceil(sizeof(Q)/dnum)
   * 2 - Extend ciphertext modulus from Q to Q*P
   *     Here P is a product of special primes
   * 3 - Multiply extended component with key switching key
   * 4 - Decrease the ciphertext modulus back down to Q
   *
   * It's not necessary to understand how all these stages work, as
   * long as it's clear that the size of the ciphertext modulus is
   * increased from sizeof(Q) to sizeof(Q)+sizeof(P) in stage 2. P
   * is always set to be as small as possible, as long as sizeof(P)
   * is larger than the size of the largest digit, i.e., than
   * ceil(sizeof(Q)/dnum). Therefore, the size of P is inversely
   * related to the number of digits, so the more digits we have, the
   * smaller P has to be.
   *
   * The tradeoff here is that more digits means that the digit
   * decomposition stage becomes more expensive, but the maximum
   * size of the ciphertext modulus Q*P becomes smaller. Since
   * the size of Q*P determines the necessary ring dimension to
   * achieve a certain security level, more digits can in some
   * cases mean that we can use smaller ring dimension and get
   * better performance overall.
   *
   * We show this effect with demos HybridKeySwitchingDemo1 and
   * HybridKeySwitchingDemo2.
   *
   */
  parameters.SetNumLargeDigits(dnum);
  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

  std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension()
            << std::endl;

  std::cout << "- Using HYBRID key switching with " << dnum << " digits"
            << std::endl
            << std::endl;

  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  auto keys = cc->KeyGen();
  cc->EvalAtIndexKeyGen(keys.secretKey, {1, -2});

  // Input
  std::vector<double> x = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  TimeVar t;
  TIC(t);
  auto cRot1 = cc->EvalAtIndex(c, 1);
  auto cRot2 = cc->EvalAtIndex(cRot1, -2);
  double time2digits = TOC(t);
  // Take note and compare the runtime to the runtime
  // of the same computation in the next demo.

  Plaintext result;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cRot2, &result);
  result->SetLength(batchSize);
  std::cout << "x rotate by -1 = " << result << std::endl;
  std::cout << " - 2 rotations with HYBRID (2 digits) took " << time2digits
            << "ms" << std::endl;

  /* Interested users may set the following if to 1
   * to observe the prime numbers comprising Q and P,
   * and how these change with the number of digits
   * dnum.
   */
#if 0
  const auto cryptoParamsCKKS =
      std::static_pointer_cast<CryptoParametersCKKSRNS>(
          cc->GetCryptoParameters());

  auto paramsQ = cc->GetElementParams()->GetParams();
  std::cout << "\nModuli in Q:" << std::endl;
  for (uint32_t i = 0; i < paramsQ.size(); i++) {
    // q0 is a bit larger because its default size is 60 bits.
    // One can change this by supplying the firstModSize argument
    // in genCryptoContextCKKS.
    std::cout << "q" << i << ": " << paramsQ[i]->GetModulus() << std::endl;
  }
  auto paramsQP = cryptoParamsCKKS->GetExtendedElementParams();
  std::cout << "Moduli in P: " << std::endl;
  BigInteger P = BigInteger(1);
  for (uint32_t i = 0; i < paramsQP->GetParams().size(); i++) {
    if (i > paramsQ.size()) {
      P = P * BigInteger(paramsQP->GetParams()[i]->GetModulus());
      std::cout << "p" << i - paramsQ.size() << ": "
                << paramsQP->GetParams()[i]->GetModulus() << std::endl;
    }
  }
  auto QBitLength = cc->GetModulus().GetLengthForBase(2);
  auto PBitLength = P.GetLengthForBase(2);
  std::cout << "\nQ = " << cc->GetModulus() << " (bit length: " << QBitLength
            << ")" << std::endl;
  std::cout << "P = " << P << " (bit length: " << PBitLength << ")"
            << std::endl;
  std::cout << "Total bit-length of ciphertext modulus: "
            << QBitLength + PBitLength << std::endl;
  std::cout << "Given this ciphertext modulus, a ring dimension of "
            << cc->GetRingDimension() << " gives us 128-bit security."
            << std::endl;
#endif
}

void HybridKeySwitchingDemo2() {
  /*
   * Please refer to comments in HybridKeySwitchingDemo1.
   *
   */

  std::cout << "\n\n\n ===== HybridKeySwitchingDemo2 ============= "
            << std::endl;

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(5);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);
  parameters.SetRescalingTechnique(FIXEDAUTO);

  // uint32_t ringDimension = 0;  // 0 means the library will choose it based on securityLevel
  /*
   * Here we use dnum = 3 digits. Even though 3 digits are
   * more than the two digits in the previous demo and the
   * cost of digit decomposition is higher, the increase in
   * digits means that individual digits are smaller, and we
   * can perform key switching by using only one special
   * prime in P (instead of two in the previous demo).
   *
   * This also means that the maximum size of ciphertext
   * modulus in key switching is smaller by 60 bits, and it
   * turns out that this decrease is adequate to warrant a
   * smaller ring dimension to achieve the same security
   * level (128-bits).
   *
   */
  uint32_t dnum = 3;

  parameters.SetNumLargeDigits(dnum);
  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

  // Compare the ring dimension in this demo to the one in
  // the previous.
  std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension()
            << std::endl;

  std::cout << "- Using HYBRID key switching with " << dnum << " digits"
            << std::endl
            << std::endl;

  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  auto keys = cc->KeyGen();
  cc->EvalAtIndexKeyGen(keys.secretKey, {1, -2});

  // Input
  std::vector<double> x = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  TimeVar t;
  TIC(t);
  auto cRot1 = cc->EvalAtIndex(c, 1);
  auto cRot2 = cc->EvalAtIndex(cRot1, -2);
  // The runtime here is smaller than in the previous demo.
  double time3digits = TOC(t);

  Plaintext result;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cRot2, &result);
  result->SetLength(batchSize);
  std::cout << "x rotate by -1 = " << result << std::endl;
  std::cout << " - 2 rotations with HYBRID (3 digits) took " << time3digits
            << "ms" << std::endl;

  /* Interested users may set the following if to 1
   * to observe the prime numbers comprising Q and P,
   * and how these change with the number of digits
   * dnum.
   */
#if 0
  const auto cryptoParamsCKKS =
      std::static_pointer_cast<CryptoParametersCKKSRNS>(
          cc->GetCryptoParameters());

  auto paramsQ = cc->GetElementParams()->GetParams();
  std::cout << "\nModuli in Q:" << std::endl;
  for (uint32_t i = 0; i < paramsQ.size(); i++) {
    // q0 is a bit larger because its default size is 60 bits.
    // One can change this by supplying the firstModSize argument
    // in genCryptoContextCKKS.
    std::cout << "q" << i << ": " << paramsQ[i]->GetModulus() << std::endl;
  }
  auto paramsQP = cryptoParamsCKKS->GetExtendedElementParams();
  std::cout << "Moduli in P: " << std::endl;
  BigInteger P = BigInteger(1);
  for (uint32_t i = 0; i < paramsQP->GetParams().size(); i++) {
    if (i > paramsQ.size()) {
      P = P * BigInteger(paramsQP->GetParams()[i]->GetModulus());
      std::cout << "p" << i - paramsQ.size() << ": "
                << paramsQP->GetParams()[i]->GetModulus() << std::endl;
    }
  }
  auto QBitLength = cc->GetModulus().GetLengthForBase(2);
  auto PBitLength = P.GetLengthForBase(2);
  std::cout << "\nQ = " << cc->GetModulus() << " (bit length: " << QBitLength
            << ")" << std::endl;
  std::cout << "P = " << P << " (bit length: " << PBitLength << ")"
            << std::endl;
  std::cout << "Given this ciphertext modulus, a ring dimension of "
            << cc->GetRingDimension() << " gives us 128-bit security."
            << std::endl;
#endif
}

void FastRotationsDemo1() {
  /*
   * In CKKS, whenever someone applies a rotation R() to a ciphertext
   * encrypted with key s, we get a result which is not valid under
   * key s, but under the same rotation R(s) of s. Therefore, after
   * every rotation we need to perform key switching, making them as
   * expensive as multiplications.
   *
   * As mentioned earlier (in comments of HybridKeySwitchingDemo1),
   * key switching involves the following steps:
   * 1 - Digit decomposition
   * 2 - Extend ciphertext modulus from Q to Q*P
   * 3 - Multiply extended component with key switching key
   * 4 - Decrease the ciphertext modulus back down to Q
   *
   * A useful observation is that the first two steps are independent
   * of the particular rotation we want to perform. Steps 3-4 on the
   * other hand depend on the specific rotation we have at hand,
   * because each rotation index has a different key switch key.
   *
   * This observation means that, if we want to perform multiple
   * different rotations to the same ciphertext, we can perform
   * the first two steps once, and then only perform steps 3-4 for
   * each rotation. This technique is called hoisting, and we have
   * implemented it for all three key switching techniques (BV, GHS,
   * HYBRID) in OpenFHE.
   *
   * The benefits expected by this technique differ depending on the
   * key switching algorithms we're using. BV is the technique that
   * gets the greatest benefits, because the digit decomposition is
   * the most expensive part. However, HYBRID also benefits from
   * hoisting, and we show this in this part of the demo.
   *
   */

  std::cout << "\n\n\n ===== FastRotationsDemo1 ============= " << std::endl;

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(1);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

  uint32_t N = cc->GetRingDimension();
  std::cout << "CKKS scheme is using ring dimension " << N << std::endl
            << std::endl;

  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  auto keys = cc->KeyGen();
  cc->EvalAtIndexKeyGen(keys.secretKey, {1, 2, 3, 4, 5, 6, 7});

  // Input
  std::vector<double> x = {0, 0, 0, 0, 0, 0, 0, 1};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  Ciphertext<DCRTPoly> cRot1, cRot2, cRot3, cRot4, cRot5, cRot6, cRot7;

  // First, we perform 7 regular (non-hoisted) rotations
  // and measure the runtime.
  TimeVar t;
  TIC(t);
  cRot1 = cc->EvalAtIndex(c, 1);
  cRot2 = cc->EvalAtIndex(c, 2);
  cRot3 = cc->EvalAtIndex(c, 3);
  cRot4 = cc->EvalAtIndex(c, 4);
  cRot5 = cc->EvalAtIndex(c, 5);
  cRot6 = cc->EvalAtIndex(c, 6);
  cRot7 = cc->EvalAtIndex(c, 7);
  double timeNoHoisting = TOC(t);

  auto cResNoHoist = c + cRot1 + cRot2 + cRot3 + cRot4 + cRot5 + cRot6 + cRot7;

  // M is the cyclotomic order and we need it to call EvalFastRotation
  uint32_t M = 2 * N;

  // Then, we perform 7 rotations with hoisting.
  TIC(t);
  auto cPrecomp = cc->EvalFastRotationPrecompute(c);
  cRot1 = cc->EvalFastRotation(c, 1, M, cPrecomp);
  cRot2 = cc->EvalFastRotation(c, 2, M, cPrecomp);
  cRot3 = cc->EvalFastRotation(c, 3, M, cPrecomp);
  cRot4 = cc->EvalFastRotation(c, 4, M, cPrecomp);
  cRot5 = cc->EvalFastRotation(c, 5, M, cPrecomp);
  cRot6 = cc->EvalFastRotation(c, 6, M, cPrecomp);
  cRot7 = cc->EvalFastRotation(c, 7, M, cPrecomp);
  double timeHoisting = TOC(t);
  // The time with hoisting should be faster than without hoisting.

  auto cResHoist = c + cRot1 + cRot2 + cRot3 + cRot4 + cRot5 + cRot6 + cRot7;

  Plaintext result;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cResNoHoist, &result);
  result->SetLength(batchSize);
  std::cout << "Result without hoisting = " << result << std::endl;
  std::cout << " - 7 rotations on x without hoisting took " << timeNoHoisting
            << "ms" << std::endl;

  cc->Decrypt(keys.secretKey, cResHoist, &result);
  result->SetLength(batchSize);
  std::cout << "Result with hoisting = " << result << std::endl;
  std::cout << " - 7 rotations on x with hoisting took " << timeHoisting << "ms"
            << std::endl;
}

void FastRotationsDemo2() {
  /*
   * This demo is identical to the previous one, with the exception
   * that we use BV key switching instead of HYBRID.
   *
   * The benefits expected by hoisting differ depending on the
   * key switching algorithms we're using. BV is the technique that
   * gets the greatest benefits, because the digit decomposition is
   * the most expensive part. However, HYBRID also benefits from
   * hoisting, and we show this in this part of the demo.
   *
   */

  std::cout << "\n\n\n ===== FastRotationsDemo2 ============= " << std::endl;

  uint32_t batchSize = 8;
  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(1);
  parameters.SetScalingFactorBits(90);
  parameters.SetBatchSize(batchSize);
  parameters.SetRescalingTechnique(FIXEDAUTO);
  parameters.SetKeySwitchTechnique(BV);
  /*
   * This controls how many multiplications are possible without rescaling.
   * The number of multiplications (maxRelinSkDeg) is maxDepth - 1.
   * This is useful for an optimization technique called lazy
   * re-linearization (only applicable in FIXEDMANUAL, as
   * FIXEDAUTO implements automatic rescaling).
   */
  // This is the size of the first modulus
  // by default, firstModSize is set to 105
  uint32_t firstModSize = 100;
  /*
   * The digit size is only used in BV key switching and
   * it allows us to perform digit decomposition at a finer granularity.
   * Under normal circumstances, digit decomposition is what we call
   * RNS decomposition, i.e., each digit is roughly the size of the
   * qi's that comprise the ciphertext modulus Q. When using BV, in
   * certain cases like having to perform rotations without any
   * preceding multiplication, we need to have smaller digits to prevent
   * noise from corrupting the result. In this case, using digitSize = 10
   * does the trick. Users are encouraged to set this to 0 (i.e., RNS
   * decomposition) and see how the results are incorrect.
   */
  uint32_t digitSize = 10;

  parameters.SetFirstModSize(firstModSize);
  parameters.SetDigitSize(digitSize);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

  uint32_t N = cc->GetRingDimension();
  std::cout << "CKKS scheme is using ring dimension " << N << std::endl
            << std::endl;

  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);

  auto keys = cc->KeyGen();
  cc->EvalAtIndexKeyGen(keys.secretKey, {1, 2, 3, 4, 5, 6, 7});

  // Input
  std::vector<double> x = {0, 0, 0, 0, 0, 0, 0, 1};
  Plaintext ptxt = cc->MakeCKKSPackedPlaintext(x);

  std::cout << "Input x: " << ptxt << std::endl;

  auto c = cc->Encrypt(keys.publicKey, ptxt);

  Ciphertext<DCRTPoly> cRot1, cRot2, cRot3, cRot4, cRot5, cRot6, cRot7;

  // First, we perform 7 regular (non-hoisted) rotations
  // and measure the runtime.
  TimeVar t;
  TIC(t);
  cRot1 = cc->EvalAtIndex(c, 1);
  cRot2 = cc->EvalAtIndex(c, 2);
  cRot3 = cc->EvalAtIndex(c, 3);
  cRot4 = cc->EvalAtIndex(c, 4);
  cRot5 = cc->EvalAtIndex(c, 5);
  cRot6 = cc->EvalAtIndex(c, 6);
  cRot7 = cc->EvalAtIndex(c, 7);
  double timeNoHoisting = TOC(t);

  auto cResNoHoist = c + cRot1 + cRot2 + cRot3 + cRot4 + cRot5 + cRot6 + cRot7;

  // M is the cyclotomic order and we need it to call EvalFastRotation
  uint32_t M = 2 * N;

  // Then, we perform 7 rotations with hoisting.
  TIC(t);
  auto cPrecomp = cc->EvalFastRotationPrecompute(c);
  cRot1 = cc->EvalFastRotation(c, 1, M, cPrecomp);
  cRot2 = cc->EvalFastRotation(c, 2, M, cPrecomp);
  cRot3 = cc->EvalFastRotation(c, 3, M, cPrecomp);
  cRot4 = cc->EvalFastRotation(c, 4, M, cPrecomp);
  cRot5 = cc->EvalFastRotation(c, 5, M, cPrecomp);
  cRot6 = cc->EvalFastRotation(c, 6, M, cPrecomp);
  cRot7 = cc->EvalFastRotation(c, 7, M, cPrecomp);
  double timeHoisting = TOC(t);
  /* The time with hoisting should be faster than without hoisting.
   * Also, the benefits from hoisting should be more pronounced in this
   * case because we're using BV. Of course, we also observe less
   * accurate results than when using HYBRID, because of using
   * digitSize = 10 (Users can decrease digitSize to see the accuracy
   * increase, and performance decrease).
   */

  auto cResHoist = c + cRot1 + cRot2 + cRot3 + cRot4 + cRot5 + cRot6 + cRot7;

  Plaintext result;
  std::cout.precision(8);

  cc->Decrypt(keys.secretKey, cResNoHoist, &result);
  result->SetLength(batchSize);
  std::cout << "Result without hoisting = " << result << std::endl;
  std::cout << " - 7 rotations on x without hoisting took " << timeNoHoisting
            << "ms" << std::endl;

  cc->Decrypt(keys.secretKey, cResHoist, &result);
  result->SetLength(batchSize);
  std::cout << "Result with hoisting = " << result << std::endl;
  std::cout << " - 7 rotations on x with hoisting took " << timeHoisting << "ms"
            << std::endl;
}
