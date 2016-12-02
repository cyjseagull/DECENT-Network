//
// Created by Josef Sevcik on 25/11/2016.
//
#include <decent/encrypt/encryptionutils.hpp>

#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/files.h>
#include <cryptopp/ccm.h>
#include <cryptopp/md5.h>

#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>
#include <iostream>


namespace decent{ namespace crypto{
AutoSeededRandomPool rng;

std::string d_integer::to_string() const
{
   std::ostringstream oss;
   oss<<*this;
   std::string tmp(oss.str());
   return tmp;
}


d_integer d_integer::from_string(std::string from)const
{
   CryptoPP::Integer tmp(from.c_str());
   return tmp;
}

encryption_results AES_encrypt_file(const std::string &fileIn, const std::string &fileOut, const aes_key &key) {
    try {
        byte iv[CryptoPP::AES::BLOCKSIZE];
        memset(iv, 0, sizeof(iv));
        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption e;
        CryptoPP::FileSink fs(fileOut.c_str(), true);
        e.SetKeyWithIV(key.key_byte, CryptoPP::AES::MAX_KEYLENGTH, iv);
        CryptoPP::StreamTransformationFilter* filter=new CryptoPP::StreamTransformationFilter(e, &fs);

        const char* file_name = fileIn.c_str();
        CryptoPP::FileSource* fsource= new CryptoPP::FileSource(file_name, true, filter);
    } catch (const CryptoPP::Exception &e) {
        elog(e.GetWhat());
        switch (e.GetErrorType()) {
            case CryptoPP::Exception::IO_ERROR:
                return io_error;
            default:
                return other_error;
        }
    }
    return ok;
}

encryption_results AES_decrypt_file(const std::string &fileIn, const std::string &fileOut, const aes_key &key) {
    try {
       byte iv[CryptoPP::AES::BLOCKSIZE];
       memset(iv, 0, sizeof(iv));
       CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption d;
       d.SetKeyWithIV(key.key_byte, CryptoPP::AES::MAX_KEYLENGTH, iv);
       const char* file_name = fileIn.c_str();
       CryptoPP::FileSink fs(fileOut.c_str(), true);
       CryptoPP::StreamTransformationFilter* filter = new CryptoPP::StreamTransformationFilter(d, &fs);
       CryptoPP::FileSource* fsource = new CryptoPP::FileSource(file_name, true, filter);
    } catch (const CryptoPP::Exception &e) {
        elog(e.GetWhat());
        switch (e.GetErrorType()) {
            case CryptoPP::Exception::IO_ERROR:
                return io_error;
            default:
                return other_error;
        }
    }
    return ok;
}

d_integer generate_private_el_gamal_key()
{
    CryptoPP::Integer im (rng, CryptoPP::Integer::One(), EL_GAMAL_MODULUS_512 -1);
    d_integer key = im;
    return key;
}

d_integer get_public_el_gamal_key(const d_integer &privateKey)
{
    ModularArithmetic ma(EL_GAMAL_MODULUS_512);
    d_integer publicKey = ma.Exponentiate(EL_GAMAL_GROUP_GENERATOR, privateKey);

    return publicKey;
}

encryption_results el_gamal_encrypt(const valtype &message, d_integer &publicKey, ciphertext &result)
{
    ElGamalKeys::PublicKey key;
    key.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
    key.SetPublicElement(publicKey);

    CryptoPP::Integer randomizer(rng, CryptoPP::Integer::One(), EL_GAMAL_MODULUS_512 - 1);

    try{
        byte buffer[MESSAGE_SIZE];
        FC_ASSERT(message.size() <= MESSAGE_SIZE);
        copy(message.begin(), message.end(), buffer);
        SecByteBlock messageBytes(buffer, MESSAGE_SIZE);

        ElGamal::GroupParameters groupParams;
        groupParams.Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
        ModularArithmetic mr(EL_GAMAL_MODULUS_512);

        CryptoPP::Integer m;
        m.Decode(messageBytes, MESSAGE_SIZE);

        result.D1 = groupParams.ExponentiateBase(randomizer);
        result.C1 = mr.Multiply(m, mr.Exponentiate(publicKey, randomizer));

    }catch(const CryptoPP::Exception &e) {
        elog(e.GetWhat());
        switch (e.GetErrorType()) {
            case CryptoPP::Exception::IO_ERROR:
                return io_error;
            default:
                return other_error;
        }
    }
    return ok;
}

encryption_results el_gamal_decrypt(const ciphertext &input, d_integer &privateKey, valtype &plaintext)
{
    ElGamalKeys::PrivateKey key;
    key.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
    key.SetPrivateExponent(privateKey);
    try{

        byte recovered[MESSAGE_SIZE];
        ElGamal::GroupParameters groupParams;
        groupParams.Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);

        ModularArithmetic mr(groupParams.GetModulus());

        CryptoPP::Integer s = mr.Exponentiate(input.D1, privateKey);
        CryptoPP::Integer m = mr.Multiply(input.C1, mr.MultiplicativeInverse(s));
        size_t size = m.MinEncodedSize();

        m.Encode(recovered, MESSAGE_SIZE);

        plaintext.reserve(MESSAGE_SIZE);
        std::copy(recovered, recovered+MESSAGE_SIZE, std::back_inserter(plaintext));
    }catch (const CryptoPP::Exception &e) {
        elog(e.GetWhat());
        switch (e.GetErrorType()) {
            case CryptoPP::Exception::IO_ERROR:
                return io_error;
            default:
                return other_error;
        }
    }
    return ok;
}

d_integer hash_elements(ciphertext t1, ciphertext t2, d_integer key1, d_integer key2, d_integer G1, d_integer G2, d_integer G3)
{
   //std::cout <<"hash params: "<<t1.C1.to_string()<<t1.D1.to_string()<< t2.C1.to_string()<<t2.D1.to_string()<< key1.to_string()<<key2.to_string()<<G1.to_string()<<G2.to_string()<<G3.to_string() <<"\n";

   CryptoPP::Weak1::MD5 hashier;
   size_t hashSpace =9*(EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.CreateUpdateSpace(hashSpace);
   byte tmp[EL_GAMAL_GROUP_ELEMENT_SIZE];

   t1.D1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   t1.C1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   t2.D1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   t2.C1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   key1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   key2.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   G1.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   G2.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   G3.Encode(tmp,EL_GAMAL_GROUP_ELEMENT_SIZE);
   hashier.Update(tmp, EL_GAMAL_GROUP_ELEMENT_SIZE);

   byte digest[32];
   hashier.Final(digest);
   CryptoPP::Integer x(digest, 32);

   return x;
}

bool
verify_delivery_proof(const delivery_proof &proof, const ciphertext &first, const ciphertext &second,
                      const d_integer &firstPulicKey, const d_integer &secondPublicKey)
{
   ElGamalKeys::PublicKey key1;
   key1.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
   key1.SetPublicElement(firstPulicKey);

   ElGamalKeys::PublicKey key2;
   key2.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
   key2.SetPublicElement(secondPublicKey);

   d_integer x = hash_elements(first, second, firstPulicKey, secondPublicKey, proof.G1, proof.G2, proof.G3);
   //std::cout <<"hash inside verify proof is: "<<x.to_string()<<"\n";

   ElGamal::GroupParameters groupParams;
   ModularArithmetic mr(EL_GAMAL_MODULUS_512);

   d_integer t1 = mr.Exponentiate(EL_GAMAL_GROUP_GENERATOR, proof.s);
   d_integer t2 = mr.Multiply(mr.Exponentiate(key1.GetPublicElement(), x), proof.G1);

   //std::cout << "T1: "<<t1.to_string()<<"\nT2: "<<t2.to_string()<<"\n";

   if(t1 != t2)
      return false;
   if (mr.Exponentiate(key1.GetGroupParameters().GetSubgroupGenerator(), proof.r) != mr.Multiply(mr.Exponentiate(second.D1, x), proof.G2))
      return false;

   CryptoPP::Integer c2v = mr.Exponentiate(second.C1, x);
   CryptoPP::Integer c1v = mr.MultiplicativeInverse(mr.Exponentiate(first.C1, x));
   CryptoPP::Integer d1v = mr.Exponentiate(first.D1, proof.s);
   CryptoPP::Integer r2v = mr.Exponentiate(key2.GetPublicElement(), proof.r);
   CryptoPP::Integer c2vc1v =  mr.Multiply(c2v, c1v);
   CryptoPP::Integer c2vc1vd1v = mr.Multiply(c2vc1v, d1v);

   return mr.Multiply(c2vc1vd1v, mr.MultiplicativeInverse(r2v)) == proof.G3;
}



encryption_results
encrypt_with_proof(const valtype &message, const d_integer &privateKey, const d_integer &destinationPublicKey,
                   const ciphertext &incoming, ciphertext &outgoing, delivery_proof &proof)
{
   FC_ASSERT(message.size() == MESSAGE_SIZE);
   try{
      ElGamalKeys::PrivateKey private_key;
      private_key.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
      private_key.SetPrivateExponent(privateKey);

      ElGamalKeys::PublicKey public_key;
      public_key.AccessGroupParameters().Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
      public_key.SetPublicElement(destinationPublicKey);


      CryptoPP::Integer randomizer(rng, CryptoPP::Integer::One(), EL_GAMAL_MODULUS_512 - 1);

      byte messageBytes[MESSAGE_SIZE];
      copy(message.begin(), message.end(), messageBytes);

      //encrypt message to outgoing
      ElGamal::GroupParameters groupParams;
      groupParams.Initialize(EL_GAMAL_MODULUS_512, EL_GAMAL_GROUP_GENERATOR);
      ModularArithmetic mr(EL_GAMAL_MODULUS_512);


      CryptoPP::Integer m;
      m.Decode(messageBytes, MESSAGE_SIZE);
      outgoing.D1 = groupParams.ExponentiateBase(randomizer);
      outgoing.C1 = mr.Multiply(m, mr.Exponentiate(destinationPublicKey, randomizer));
      //create delivery proof to proofOfDelivery
      CryptoPP::Integer subgroupOrder = groupParams.GetSubgroupOrder();
      CryptoPP::Integer subGroupGenerator = groupParams.GetSubgroupGenerator();
      CryptoPP::Integer privateExponent =  private_key.GetPrivateExponent();
      CryptoPP::Integer myPublicElement = groupParams.ExponentiateBase(privateExponent);

      CryptoPP::Integer b1(rng, CryptoPP::Integer::One(), subgroupOrder-1);
      CryptoPP::Integer b2(rng, CryptoPP::Integer::One(), subgroupOrder-1);

      proof.G1 = mr.Exponentiate(subGroupGenerator, b1);
      proof.G2 = mr.Exponentiate(subGroupGenerator, b2);
      proof.G3 = mr.Multiply(mr.Exponentiate(incoming.D1, b1), mr.MultiplicativeInverse(mr.Exponentiate(destinationPublicKey, b2)));

      d_integer x = hash_elements(incoming, outgoing, myPublicElement, public_key.GetPublicElement(), proof.G1, proof.G2, proof.G3);

      //std::cout <<"hash inside encrypt with proof is: "<<x.to_string()<<"\n";
      proof.s = privateExponent * x + b1;
      proof.r = randomizer * x + b2;

   }catch(const CryptoPP::Exception &e) {
      elog(e.GetWhat());
      switch (e.GetErrorType()) {
         case CryptoPP::Exception::IO_ERROR:
            return io_error;
         default:
            return other_error;
      }
   }
   return ok;
}

void shamir_secret::calculate_split()
{
   split.clear();
   std::vector<d_integer> coef;
   coef.reserve(quorum);
   coef.push_back(secret);

   ModularArithmetic mr(SHAMIR_ORDER);
   for(int i=1; i<quorum; i++)
   {
      CryptoPP::Integer a(rng, CryptoPP::Integer::One(), SHAMIR_ORDER);
      coef.push_back(a);
   }

   for(d_integer x=d_integer::One(); x<=shares; x++)
   {
      d_integer y = coef[0];
      for (int i=1; i<quorum; i++)
         y = mr.Add(y, mr.Multiply(coef[i],mr.Exponentiate(x,i)));
      split.push_back(std::make_pair(x,y));
   }
}

void shamir_secret::calculate_secret()
{
   d_integer res = d_integer::Zero();
   ModularArithmetic mr(SHAMIR_ORDER);

   for( const auto& s: split )
   {
      d_integer numerator = d_integer::One();
      d_integer denominator = d_integer::One();
      for (const auto& p: split)
      {
         if ( p == s )
            continue;
         d_integer startposition = s.first;
         d_integer nextposition = mr.Inverse(p.first);
         numerator = mr.Multiply(nextposition, numerator);
         denominator = mr.Multiply(mr.Add(startposition, nextposition), denominator);
      }
      res = mr.Add(mr.Multiply(mr.Multiply(mr.MultiplicativeInverse(denominator), numerator), s.second), res);
   }
   secret = res;
}


}}//namespace