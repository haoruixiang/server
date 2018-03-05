#include "rsa_.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <glog/logging.h>
EasyRSA::EasyRSA(int n)
{
    m_piv_rsa = RSA_generate_key(n, 65537, NULL, NULL);
    BIO *pub = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPublicKey(pub, m_piv_rsa); 
    int pub_len = BIO_pending(pub);
    char pub_key[pub_len + 1];
    BIO_read(pub, pub_key, pub_len);
    pub_key[pub_len] = 0;
    m_public_key = std::string(pub_key, pub_len);
    m_pub_rsa = 0;
    BIO_free_all(pub);
    SetPublicKey(m_public_key);
}
EasyRSA::~EasyRSA()
{
    RSA_free(m_piv_rsa);
    if (m_pub_rsa){
        RSA_free(m_pub_rsa);
    }
}
std::string EasyRSA::GetPublicKey()
{
    return m_public_key;
}

bool  EasyRSA::SetPublicKey(const std::string & key)
{
    if (m_pub_rsa){
        RSA_free(m_pub_rsa);
        m_pub_rsa = 0;
    }
    BIO *keybio = BIO_new_mem_buf((char*)key.c_str(), -1);
    if (!keybio){
        LOG(ERROR)<<"BIO_new_mem_buf false";
        return false;
    }
    m_pub_rsa = PEM_read_bio_RSAPublicKey(keybio, NULL, NULL, NULL);
    if (!m_pub_rsa){
        ERR_load_crypto_strings();  
        char errBuf[512];  
        ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));  
        LOG(ERROR)<<"PEM_read_bio_RSA_PUBKEY false:"<<errBuf;
        return false;
    }
    BIO_free_all(keybio);
    return true;
}

std::string EasyRSA::Encode(std::string & msg)
{
    if (!m_pub_rsa){
        LOG(ERROR)<<"RSA_public_encrypt error";
        return std::string();
    }
    int rsa_len = RSA_size(m_pub_rsa);
    int clen = rsa_len/2;
    int n = msg.size()/clen;
    std::string rmsg;
    const char * cp = msg.c_str();
    for (int i=0; i<=n; i++){
        char tmsg[clen+1];
        memset(tmsg, 0, sizeof(tmsg));
        int move = i*clen;
        int tlen = clen;
        if ((int)(msg.length() - move) > clen ){
            memcpy(tmsg, cp + move, clen);
        } else {
            memcpy(tmsg, cp + move, msg.length() - move);
            tlen = msg.length() - move;
        }
        char en[rsa_len+1];  
        memset(en, 0, rsa_len + 1);  
        int len = RSA_public_encrypt(tlen, (unsigned char*)tmsg, (unsigned char*)en, m_pub_rsa, RSA_PKCS1_OAEP_PADDING);
        if (len < 0) {
            LOG(ERROR)<<"RSA_public_encrypt error rsa_len:"<<rsa_len<<" msg len:"<<msg.size();
            return rmsg;
        }
        LOG(INFO)<<"rsa_len:"<<rsa_len<<" from:"<< tlen<<" tlen:"<< len<<" move:"<<move<<" "<<tmsg;;
        rmsg.append(en,len);
    }
    return rmsg;
}

std::string EasyRSA::Decode(std::string & msg)
{
    int rsa_len = RSA_size(m_piv_rsa);
    int n = msg.size()/rsa_len;
    std::string rmsg;
    const char * cp = msg.c_str();
    for (int i=0; i<=n; i++){
        char tmsg[rsa_len+1];
        memset(tmsg, 0, sizeof(tmsg));
        int move = i*rsa_len;
        LOG(INFO)<<"len:"<<msg.size()<<" move:"<<move;
        //int tlen = rsa_len;
        if ((int)(msg.length() - move) > rsa_len ){
            memcpy(tmsg, cp + move, rsa_len);
        } else {
            memcpy(tmsg, cp + move, msg.length() - move);
            //tlen = msg.length() - move;
        }
        char de[rsa_len+1];
        memset(de, 0, rsa_len + 1);
        int len = RSA_private_decrypt(rsa_len, (unsigned char*)tmsg, (unsigned char*)de, m_piv_rsa, RSA_PKCS1_OAEP_PADDING);
        if (len < 0){
            LOG(ERROR)<<"RSA_private_decrypt error";
            return rmsg;
        }
        LOG(INFO)<<de;
        rmsg.append(de, len);
    }
    return rmsg;
}

