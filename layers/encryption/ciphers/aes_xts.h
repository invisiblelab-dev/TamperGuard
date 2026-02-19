#ifndef __AES_XTS_H__
#define __AES_XTS_H__

#include "../../../logdef.h"
#include <stddef.h>

int aes_xts_encrypt(const unsigned char *key, const unsigned char *iv,
                    const unsigned char *data, int data_len,
                    unsigned char *encrypted_data);

int aes_xts_decrypt(const unsigned char *key, const unsigned char *iv,
                    const unsigned char *encrypted_data, int encrypted_data_len,
                    unsigned char *data);

#endif
