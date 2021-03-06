/*
 * Copyright (c) 2015-2016, Avionic Design GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Avionic Design GmbH nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <iomanip>
using std::hex;

#include <string>
using std::string;

#include <cstdlib>
using std::exit;

#define CRYPTOPP_INCLUDE_CRYPTLIB	<CRYPTOLIB_HEADER_PREFIX/cryptlib.h>
#define CRYPTOPP_INCLUDE_FILES		<CRYPTOLIB_HEADER_PREFIX/files.h>
#define CRYPTOPP_INCLUDE_FILTERS	<CRYPTOLIB_HEADER_PREFIX/filters.h>
#define CRYPTOPP_INCLUDE_INTEGER	<CRYPTOLIB_HEADER_PREFIX/integer.h>
#define CRYPTOPP_INCLUDE_OSRNG		<CRYPTOLIB_HEADER_PREFIX/osrng.h>
#define CRYPTOPP_INCLUDE_QUEUE		<CRYPTOLIB_HEADER_PREFIX/queue.h>
#define CRYPTOPP_INCLUDE_RSA		<CRYPTOLIB_HEADER_PREFIX/rsa.h>
#define CRYPTOPP_INCLUDE_PSSR		<CRYPTOLIB_HEADER_PREFIX/pssr.h>
#define CRYPTOPP_INCLUDE_SHA		<CRYPTOLIB_HEADER_PREFIX/sha.h>
#define CRYPTOPP_INCLUDE_SECBLOCK	<CRYPTOLIB_HEADER_PREFIX/secblock.h>

#include CRYPTOPP_INCLUDE_CRYPTLIB
#include CRYPTOPP_INCLUDE_FILES
#include CRYPTOPP_INCLUDE_FILTERS
#include CRYPTOPP_INCLUDE_INTEGER
#include CRYPTOPP_INCLUDE_OSRNG
#include CRYPTOPP_INCLUDE_QUEUE
#include CRYPTOPP_INCLUDE_RSA
#include CRYPTOPP_INCLUDE_PSSR
#include CRYPTOPP_INCLUDE_SHA
#include CRYPTOPP_INCLUDE_SECBLOCK

using CryptoPP::Exception;
using CryptoPP::Integer;
using CryptoPP::FileSource;
using CryptoPP::StringSink;
using CryptoPP::SignerFilter;
using CryptoPP::ByteQueue;
using CryptoPP::RSA;
using CryptoPP::RSASS;
using CryptoPP::PSS;
using CryptoPP::SHA256;
using CryptoPP::SecByteBlock;
using CryptoPP::AutoSeededRandomPool;

#include "rsa-pss.h"
#include <stdexcept>
#include "rcm.h"

extern "C" int rsa_pss_sign(const char *key_file, const unsigned char *msg,
			int len, unsigned char *sig_buf, unsigned char *modulus_buf)
{
	try {
		AutoSeededRandomPool rng;
		FileSource file(key_file, true);
		RSA::PrivateKey key;
		ByteQueue bq;

		// Load the key
		file.TransferTo(bq);
		bq.MessageEnd();
		key.BERDecodePrivateKey(bq, false, bq.MaxRetrievable());

		// Write the modulus
		Integer mod = key.GetModulus();
		// error check
		if (mod.ByteCount() != RCM_RSA_MODULUS_SIZE)
			throw std::length_error("incorrect rsa key modulus length");
		for (int i = 0; i < mod.ByteCount(); i++)
			modulus_buf[i] = mod.GetByte(i);

		// Sign the message
		RSASS<PSS, SHA256>::Signer signer(key);
		size_t length = signer.MaxSignatureLength();
		SecByteBlock signature(length);

		length = signer.SignMessage(rng, msg, len, signature);

		// Copy in reverse order
		for (int i = 0; i < length; i++)
			sig_buf[length - i - 1] = signature[i];
	}
	catch(const CryptoPP::Exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	catch(std::length_error& le) {
		cerr << "Error: " << le.what() << endl;
		return 1;
	}

	return 0;
}

extern "C" int rsa_pss_sign_file(const char *key_file, const char *msg_file,
			unsigned char *sig_buf)
{
	try {
		AutoSeededRandomPool rng;
		FileSource file(key_file, true);
		RSA::PrivateKey key;
		ByteQueue bq;

		// Load the key
		file.TransferTo(bq);
		bq.MessageEnd();
		key.BERDecodePrivateKey(bq, false, bq.MaxRetrievable());

		// Sign the message
		RSASS<PSS, SHA256>::Signer signer(key);
		string signature;
		FileSource src(msg_file, true,
			new SignerFilter(rng, signer,
					new StringSink(signature)));
		int length = signature.length();
		// error check
		if (length != RCM_RSA_SIG_SIZE)
			throw std::length_error("incorrect rsa key length");

		// Copy in reverse order
		for (int i = 0; i < length; i++)
			sig_buf[length - i - 1] = signature[i];
	}
	catch(const CryptoPP::Exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	catch(std::length_error& le) {
		cerr << "Error: " << le.what() << endl;
		return 1;
	}

	return 0;
}
