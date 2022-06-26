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

Example for CKKS bootstrapping

 */

#define PROFILE

#include "openfhe.h"
#include "scheme/ckksrns/cryptocontext-ckksrns.h"
#include "gen-cryptocontext.h"

using namespace std;
using namespace lbcrypto;

void BootstrapExample(MODE mode, uint32_t n, uint32_t slots, uint32_t levelsRemaining);
// same example with verbose console output removed
void BootstrapExampleClean(MODE mode, uint32_t n, uint32_t slots, uint32_t levelsRemaining);

int main(int argc, char* argv[]) {

// MODE - secret key distribution
// SPARSE - sparse ternary secrets (with h = 64)
// OPTIMIZED - uniform ternary secrets

// low-security examples
	BootstrapExample(SPARSE,1<<12,1<<11,10);
//	BootstrapExample(SPARSE,1<<12,1<<10,10);
//	BootstrapExample(OPTIMIZED,1<<12,1<<11,10);
//	BootstrapExample(OPTIMIZED,1<<12,1<<10,10);


//	BootstrapExample(SPARSE,1<<17,1<<16,10);
//	BootstrapExample(SPARSE,1<<17,1<<15,10);
//	BootstrapExample(OPTIMIZED,1<<17,1<<16,10);
//	BootstrapExample(OPTIMIZED,1<<17,1<<15,10);

	return 0;
}

void BootstrapExample(MODE mode, uint32_t n, uint32_t slots, uint32_t levelsRemaining) {

	TimeVar t;
	double timeKeyGen(0.0);
	double timePrecomp(0.0);
	double timeBootstrap(0.0);

	// giant step for baby-step-giant-step algorithm in linear transforms for encoding and decoding, respectively
	// Choose this a power of 2 preferably, otherwise an exact divisor of the number of elements in the sum
	std::vector<uint32_t> dim1 = { 0, 0 };

	// budget in levels for FFT for encoding and decoding, respectively
	// Choose a number smaller than ceil(log2(slots))
	std::vector<uint32_t> levelBudget = { 4, 4 };

#if NATIVEINT==128
  RescalingTechnique rescaleTech = FIXEDMANUAL;
  usint dcrtBits = 78;
  usint firstMod = 89; /*firstMod*/
#else
  RescalingTechnique rescaleTech = FLEXIBLEAUTO;
  usint dcrtBits = 59;
  usint firstMod = 60; /*firstMod*/
#endif

	// computes how many levels are needed for
	uint32_t approxModDepth = 9;
	uint32_t r = 6;
	if (mode == OPTIMIZED) {
	    if (rescaleTech == FIXEDMANUAL)
	      approxModDepth += r - 1;
	    else
	      approxModDepth += r;
	}

	usint depth = levelsRemaining + approxModDepth + levelBudget[0] + levelBudget[1];


  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(depth);
  parameters.SetScalingFactorBits(dcrtBits);
  parameters.SetRescalingTechnique(rescaleTech);
  parameters.SetRingDim(n);
  parameters.SetMode(mode);
  parameters.SetNumLargeDigits(3);
  parameters.SetSecurityLevel(HEStd_NotSet);
  parameters.SetKeySwitchTechnique(HYBRID);
  parameters.SetFirstModSize(firstMod);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

#if 0
	CryptoContext<DCRTPoly> cc =
		CryptoContextFactory<DCRTPoly>::genCryptoContextCKKS(
			   depth,
			   dcrtBits,
			   slots,
			   HEStd_NotSet,
			   n, /*ringDimension*/
			   rescaleTech,
			   HYBRID,
			   3, /*numLargeDigits*/
			   2, /*maxDepth*/
#if NATIVEINT==128
			   89, /*firstMod*/
#else
			   60, /*firstMod*/
#endif
			   0,
			   mode);
#endif

	//Turn on features
  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
	cc->Enable(LEVELEDSHE);
	cc->Enable(ADVANCEDSHE);
	cc->Enable(FHE);

	const shared_ptr<CryptoParametersCKKSRNS> cryptoParams =
				std::static_pointer_cast<CryptoParametersCKKSRNS>(cc->GetCryptoParameters());

	std::cout << "p = " << cryptoParams->GetPlaintextModulus()
			<< std::endl;
	std::cout << "n = "
			<< cryptoParams->GetElementParams()->GetCyclotomicOrder()
					/ 2 << std::endl;
	std::cout << "# slots = "
			<< slots << std::endl;
	std::cout << "log2 q = "
			<< cryptoParams->GetElementParams()->GetModulus().GetMSB()
			<< std::endl;
	std::cout << "log2 q*p = "
			<< cryptoParams->GetParamsQP()->GetModulus().GetMSB()
			<< std::endl;

	std::cout << "dim1 = " << dim1 << std::endl;
	std::cout << "level budget = " << levelBudget << std::endl;

	TIC(t);

	// precomputations for bootstrapping
	cc->EvalBootstrapSetup(levelBudget, dim1, slots);

//	std::cout << "gEnc = " << cc->GetGiantStepEnc() << std::endl;
//	std::cout << "# rot Enc = " << cc->GetNumRotationsEnc() << std::endl;
//	std::cout << "gEncRem = " << cc->GetGiantStepRemEnc() << std::endl;
//	std::cout << "W rot EncRem = " << cc->GetNumRotationsRemEnc() << std::endl;
//	std::cout << "gDec = " << cc->GetGiantStepDec() << std::endl;
//	std::cout << "# rot Dec = " << cc->GetNumRotationsDec() << std::endl;
//	std::cout << "gDecRem = " << cc->GetGiantStepRemDec() << std::endl;
//	std::cout << "W rot DecRem = " << cc->GetNumRotationsRemDec() << std::endl;

	timePrecomp = TOC(t);

	std::cout << "\nLinear transform precomputation time: "
			<< timePrecomp / 1000.0 << " s" << std::endl;

	auto keyPair = cc->KeyGen();

	// is the packing sparse or full?
	int32_t sparseFlag = (slots < n/2) ? 1 : 0;

	// generation of all keys needed for bootstrapping
	TIC(t);

	cc->EvalBootstrapKeyGen(keyPair.secretKey, sparseFlag);

	timeKeyGen = TOC(t);

	std::cout << "\nAutomorphism key generation time: " << timeKeyGen / 1000.0
			<< " s" << std::endl;

	cc->EvalMultKeyGen(keyPair.secretKey);

	std::vector<std::complex<double>> a( {  0.111111, 0.222222, 0.333333,
			0.444444, 0.555555, 0.666666, 0.777777, 0.888888});

	size_t encodedLength = a.size();

	std::vector<std::complex<double>> input(Fill(a,n/2));

	Plaintext plaintext1 = cc->MakeCKKSPackedPlaintext(input, 1, depth - 1);

	auto ciphertext1 = cc->Encrypt(keyPair.publicKey, plaintext1);

	std::cerr << "\nNumber of levels before bootstrapping: "
			<< ciphertext1->GetElements()[0].GetNumOfElements() - 1
			<< std::endl;

	// bootstrapping operation itself

	TIC(t);

	auto ciphertextAfter = cc->EvalBootstrap(ciphertext1);

	timeBootstrap = TOC(t);

	std::cout << "\nBootstrapping time: " << timeBootstrap / 1000.0 << " s"
			<< std::endl;

	std::cerr << "\nNumber of levels consumed: "
			<< depth - ciphertextAfter->GetElements()[0].GetNumOfElements()
					+ ciphertextAfter->GetDepth() << std::endl;

	std::cerr << "\nNumber of levels remaining: "
			<< ciphertextAfter->GetElements()[0].GetNumOfElements()
					- ciphertextAfter->GetDepth() << std::endl;

	Plaintext result;
  std::cerr << "ciphertextAfter level: " << ciphertextAfter->GetLevel() << std::endl;
  std::cerr << "ciphertextAfter depth: " << ciphertextAfter->GetDepth() << std::endl;
  std::cerr << "ciphertextAfter    sf: " << ciphertextAfter->GetScalingFactor() << std::endl;
	cc->Decrypt(keyPair.secretKey, ciphertextAfter, &result);
  std::cerr << "encodedLength: " << encodedLength << std::endl;

	result->SetLength(encodedLength);
	plaintext1->SetLength(encodedLength);

	std::cout << "\nEncrypted text before bootstrapping \n\t" << plaintext1
			<< std::endl;

	std::cout << "\nEncrypted text after bootstrapping \n\t" << result
			<< std::endl;

	double error = 0;
	for (size_t i = 0; i < encodedLength; i++) {
		error = error
				+ std::fabs(
						(result->GetCKKSPackedValue()[i].real()
								- plaintext1->GetCKKSPackedValue()[i].real())
								/ plaintext1->GetCKKSPackedValue()[i].real());
	}

	std::cout << "\nAverage error: " << error / double(encodedLength)
			<< std::endl;
	std::cout << "\nAverage error - in bits: "
			<< std::log2(error / double(encodedLength)) << std::endl;

}


void BootstrapExampleClean(MODE mode, uint32_t n, uint32_t slots, uint32_t levelsRemaining) {

	// giant step for baby-step-giant-step algorithm in linear transforms for encoding and decoding, respectively
	// Choose this a power of 2 preferably, otherwise an exact divisor of the number of elements in the sum
	std::vector<uint32_t> dim1 = { 0, 0 };

	// budget in levels for FFT for encoding and decoding, respectively
	// Choose a number smaller than ceil(log2(slots))
	std::vector<uint32_t> levelBudget = { 4, 4 };

#if NATIVEINT==128
	RescalingTechnique rescaleTech = FIXEDMANUAL;
  usint dcrtBits = 78;
  usint firstMod = 89; /*firstMod*/
#else
	RescalingTechnique rescaleTech = FLEXIBLEAUTO;
	usint dcrtBits = 59;
  usint firstMod = 60; /*firstMod*/
#endif

	// computes how many levels are needed for
	uint32_t approxModDepth = 9;
	uint32_t r = 6;
	if (mode == OPTIMIZED) {
	    if (rescaleTech == FIXEDMANUAL)
	      approxModDepth += r - 1;
	    else
	      approxModDepth += r;
	}

	usint depth = levelsRemaining + approxModDepth + levelBudget[0] + levelBudget[1];

  CCParams<CryptoContextCKKSRNS> parameters;
  parameters.SetMultiplicativeDepth(depth);
  parameters.SetScalingFactorBits(dcrtBits);
  parameters.SetRescalingTechnique(rescaleTech);
  parameters.SetRingDim(n);
  parameters.SetMode(mode);
  parameters.SetNumLargeDigits(3);
  parameters.SetSecurityLevel(HEStd_NotSet);
  parameters.SetKeySwitchTechnique(HYBRID);
  parameters.SetFirstModSize(firstMod);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

#if 0
	CryptoContext<DCRTPoly> cc =
		CryptoContextFactory<DCRTPoly>::genCryptoContextCKKS(
			   depth,
			   dcrtBits,
			   slots,
			   HEStd_NotSet,
			   n, /*ringDimension*/
			   rescaleTech,
			   HYBRID,
			   3, /*numLargeDigits*/
			   2, /*maxDepth*/
#if NATIVEINT==128
			   89, /*firstMod*/
#else
			   60, /*firstMod*/
#endif
			   0,
			   mode);
#endif

	//Turn on features
	cc->Enable(PKE);
	cc->Enable(KEYSWITCH);
	cc->Enable(LEVELEDSHE);
	cc->Enable(ADVANCEDSHE);
	cc->Enable(FHE);

	// precomputations for bootstrapping
	cc->EvalBootstrapSetup(levelBudget, dim1, slots);

	// keypair is generated
	auto keyPair = cc->KeyGen();

	// generation of evaluation keys
	int32_t sparseFlag = (slots < n/2) ? 1 : 0;
	cc->EvalBootstrapKeyGen(keyPair.secretKey, sparseFlag);

	cc->EvalMultKeyGen(keyPair.secretKey);

	std::vector<std::complex<double>> a( {  0.111111, 0.222222, 0.333333,
			0.444444, 0.555555, 0.666666, 0.777777, 0.888888});

	size_t encodedLength = a.size();

	std::vector<std::complex<double>> input(Fill(a,n/2));

	Plaintext plaintext1 = cc->MakeCKKSPackedPlaintext(input, 1, depth - 1);

	auto ciphertext1 = cc->Encrypt(keyPair.publicKey, plaintext1);

	// bootstrapping operation
	auto ciphertextAfter = cc->EvalBootstrap(ciphertext1);

	Plaintext result;

	cc->Decrypt(keyPair.secretKey, ciphertextAfter, &result);

	result->SetLength(encodedLength);
	plaintext1->SetLength(encodedLength);

	std::cout << "\nEncrypted text before bootstrapping \n\t" << plaintext1
			<< std::endl;

	std::cout << "\nEncrypted text after bootstrapping \n\t" << result
			<< std::endl;

}
