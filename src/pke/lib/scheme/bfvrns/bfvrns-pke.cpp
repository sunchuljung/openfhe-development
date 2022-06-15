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
Description:

This code implements RNS variants of the Cheon-Kim-Kim-Song scheme.

The CKKS scheme is introduced in the following paper:
- Jung Hee Cheon, Andrey Kim, Miran Kim, and Yongsoo Song. Homomorphic
encryption for arithmetic of approximate numbers. Cryptology ePrint Archive,
Report 2016/421, 2016. https://eprint.iacr.org/2016/421.

 Our implementation builds from the designs here:
 - Marcelo Blatt, Alexander Gusev, Yuriy Polyakov, Kurt Rohloff, and Vinod
Vaikuntanathan. Optimized homomorphic encryption solution for secure genomewide
association studies. Cryptology ePrint Archive, Report 2019/223, 2019.
https://eprint.iacr.org/2019/223.
 - Andrey Kim, Antonis Papadimitriou, and Yuriy Polyakov. Approximate
homomorphic encryption with reduced approximation error. Cryptology ePrint
Archive, Report 2020/1118, 2020. https://eprint.iacr.org/2020/
1118.
 */

#define PROFILE

#include "cryptocontext.h"
#include "scheme/bfvrns/bfvrns-cryptoparameters.h"
#include "scheme/bfvrns/bfvrns-pke.h"

namespace lbcrypto {

Ciphertext<DCRTPoly> PKEBFVRNS::Encrypt(DCRTPoly ptxt,
    const PrivateKey<DCRTPoly> privateKey) const {
  Ciphertext<DCRTPoly> ciphertext(
      std::make_shared<CiphertextImpl<DCRTPoly>>(privateKey));

  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersBFVRNS>(
          privateKey->GetCryptoParameters());

  const auto elementParams = cryptoParams->GetElementParams();

  std::shared_ptr<std::vector<DCRTPoly>> ba =
      EncryptZeroCore(privateKey, elementParams);

  ptxt.SetFormat(Format::EVALUATION);

  const std::vector<NativeInteger> &QDivtModq = cryptoParams->GetQDivtModq();
  (*ba)[0] += ptxt.Times(QDivtModq);

  ciphertext->SetElements({std::move((*ba)[0]), std::move((*ba)[1])});
  ciphertext->SetDepth(1);

  return ciphertext;
}

Ciphertext<DCRTPoly> PKEBFVRNS::Encrypt(DCRTPoly ptxt,
    const PublicKey<DCRTPoly> publicKey) const {
  Ciphertext<DCRTPoly> ciphertext(
      std::make_shared<CiphertextImpl<DCRTPoly>>(publicKey));

  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersBFVRNS>(
          publicKey->GetCryptoParameters());

  const auto elementParams = cryptoParams->GetElementParams();

  std::shared_ptr<std::vector<DCRTPoly>> ba =
      EncryptZeroCore(publicKey, elementParams, DggType());

  ptxt.SetFormat(Format::EVALUATION);

  const std::vector<NativeInteger> &QDivtModq = cryptoParams->GetQDivtModq();
  (*ba)[0] += ptxt.Times(QDivtModq);

  ciphertext->SetElements({std::move((*ba)[0]), std::move((*ba)[1])});
  ciphertext->SetDepth(1);

  return ciphertext;
}

DecryptResult PKEBFVRNS::Decrypt(ConstCiphertext<DCRTPoly> ciphertext,
    const PrivateKey<DCRTPoly> privateKey, NativePoly *plaintext) const {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersBFVRNS>(
          privateKey->GetCryptoParameters());

  const std::vector<DCRTPoly> &cv = ciphertext->GetElements();
  DCRTPoly b = DecryptCore(cv, privateKey);
  b.SetFormat(Format::COEFFICIENT);

  if (cryptoParams->GetMultiplicationTechnique() == HPS
   || cryptoParams->GetMultiplicationTechnique() == HPSPOVERQ
   || cryptoParams->GetMultiplicationTechnique() == HPSPOVERQLEVELED) {
    *plaintext = b.ScaleAndRound(cryptoParams->GetPlaintextModulus(),
                                 cryptoParams->GettQHatInvModqDivqModt(),
                                 cryptoParams->GettQHatInvModqDivqModtPrecon(),
                                 cryptoParams->GettQHatInvModqBDivqModt(),
                                 cryptoParams->GettQHatInvModqBDivqModtPrecon(),
                                 cryptoParams->GettQHatInvModqDivqFrac(),
                                 cryptoParams->GettQHatInvModqBDivqFrac());
  } else {
    *plaintext = b.ScaleAndRound(
        cryptoParams->GetModuliQ(), cryptoParams->GetPlaintextModulus(),
        cryptoParams->Gettgamma(), cryptoParams->GettgammaQHatInvModq(),
        cryptoParams->GettgammaQHatInvModqPrecon(),
        cryptoParams->GetNegInvqModtgamma(),
        cryptoParams->GetNegInvqModtgammaPrecon());
  }

  return DecryptResult(plaintext->GetLength());
}

}  // namespace lbcrypto
