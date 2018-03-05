#ifndef  __HRSA_H__
#define  __HRSA_H__
#include <string.h>
#include <string>
#include <openssl/rsa.h>

class EasyRSA
{
public:
    EasyRSA(int n = 1024);
    ~EasyRSA();
    std::string GetPublicKey();
    bool  SetPublicKey(const std::string & key);
    std::string Encode(std::string & msg);
    std::string Decode(std::string & msg);
private:
    RSA * m_piv_rsa;
    RSA * m_pub_rsa;
    std::string m_public_key;
};

#endif
