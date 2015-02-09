/* $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/openswan-2.6.22/testing/utils/ike-scan/sha1.h#1 $
 *
 * C Header file for Steve Reid's SHA1 implementation.
 *
 * Added for ike-scan by Roy Hills.
 */

#ifndef SHA1_H
#define SHA1_H

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(uint32_t state[5], unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned int len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);

#endif /* SHA1_H */
