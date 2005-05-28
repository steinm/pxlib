#ifndef __PX_CRYPT_H
#define __PX_CRYPT_H 1

static void px_encrypt_chunk(unsigned char src[256], unsigned char dst[256],
                             unsigned char a, unsigned char b,
                             unsigned char c, unsigned char d);

static void px_encrypt_db_block(unsigned char *src, unsigned char *dest,
                                unsigned long encryption, unsigned long blocksize,
                                unsigned long blockno);

static void px_encrypt_mb_block(unsigned char *src, unsigned char *dest,
                                unsigned long encryption, unsigned long blocksize);

static void px_decrypt_chunk(unsigned char src[256], unsigned char dst[256],
                             unsigned char a, unsigned char b,
                             unsigned char c, unsigned char d);

static void px_decrypt_db_block(unsigned char *src, unsigned char *dest,
                                unsigned long encryption, unsigned long blocksize,
                                unsigned long blockno);

static void px_decrypt_mb_block(unsigned char *src, unsigned char *dest,
                                unsigned long encryption, unsigned long blocksize);

#endif /* __PX_CRYPT_H */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker noet
 * vim<600: sw=4 ts=4
 */
