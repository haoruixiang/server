#include "util/rsa/rsa_.h"
#include "server_handler.h"
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/err.h>

int main(int argc, char**argv)
{
    std::string cleartext = "aaaa中国北京12345$abcde%ABCDE@！！！!bbbbbbbbbbbbcdsadaskdjau9w0jqdqawsjdqwdjasopdko-q2dkoawsdkasp[dkqw0kdaspdkas'lkap[wqdkawsdlkasdl'qwpdlawsdlka;s";
    LOG(INFO)<<cleartext;
    EasyRSA a(1024);
    LOG(INFO)<<a.GetPublicKey();
    std::string b = a.Encode(cleartext);
    LOG(INFO)<<b.length();
    LOG(INFO)<<a.Decode(b);
    return 0;
    if (cleartext.length() > 256) {  
        LOG(INFO)<<"cleartext too length!!!";  
        return -1;  
    }
    //RSA_test2(cleartext);  
    LOG(INFO)<<"ok!!!";  
    return 0;
}
