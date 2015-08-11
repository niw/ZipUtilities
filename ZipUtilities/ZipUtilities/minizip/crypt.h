/* crypt.h -- base code for crypt/uncrypt ZIPfile


   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant

   This code is a modified version of crypting code in Infozip distribution

   The encryption/decryption parts of this source code (as opposed to the
   non-echoing password parts) were originally written in Europe.  The
   whole source package can be freely distributed, including from the USA.
   (Prior to January 2000, re-export from the US was a violation of US law.)

   This encryption code is a direct transcription of the algorithm from
   Roger Schlafly, described by Phil Katz in the file appnote.txt.  This
   file (appnote.txt) is distributed with the PKZIP program (even in the
   version without encryption capabilities).

   If you don't need crypting in your application, just define symbols
   NOCRYPT and NOUNCRYPT.

   This code support the "Traditional PKWARE Encryption".

   The new AES encryption added on Zip format by Winzip (see the page
   http://www.winzip.com/aes_info.htm ) and PKWare PKZip 5.x Strong
   Encryption is not supported.
 
   Modified for legibility August 2015 Nolan O'Brien
*/

/***********************************************************************
 * Return the next byte in the pseudo-random sequence
 */
static int decrypt_byte(unsigned long* pkeys, const unsigned long* pcrc_32_tab)
{
    /* POTENTIAL BUG:  temp*(temp^1) may overflow in an
     * unpredictable manner on 16-bit systems; not a problem
     * with any known compiler so far, though */

    unsigned int temp = ((unsigned int)(*(pkeys+2)) & 0xffff) | 2;
    return (int)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

/***********************************************************************
 * Update the encryption keys with the next byte of plain text
 */
static int update_keys(unsigned long* pkeys, const unsigned long* pcrc_32_tab, int c)
{
#define update_keys_CRC32(c, b) ((*(pcrc_32_tab+(((int)(c) ^ (b)) & 0xff))) ^ ((c) >> 8))

    (*(pkeys+0)) = update_keys_CRC32((*(pkeys+0)), c);
    (*(pkeys+1)) += (*(pkeys+0)) & 0xff;
    (*(pkeys+1)) = (*(pkeys+1)) * 134775813L + 1;
    {
      register int keyshift = (int)((*(pkeys+1)) >> 24);
      (*(pkeys+2)) = update_keys_CRC32((*(pkeys+2)), keyshift);
    }
    return c;

#undef update_keys_CRC32
}


/***********************************************************************
 * Initialize the encryption keys and the random header according to
 * the given password.
 */
static void init_keys(const char* passwd, unsigned long* pkeys, const unsigned long* pcrc_32_tab)
{
    *(pkeys+0) = 305419896L;
    *(pkeys+1) = 591751049L;
    *(pkeys+2) = 878082192L;
    while (*passwd != '\0') {
        update_keys(pkeys,pcrc_32_tab,(int)*passwd);
        passwd++;
    }
}

#define zdecode(pkeys, pcrc_32_tab, c) \
    (update_keys(pkeys, pcrc_32_tab, c ^= decrypt_byte(pkeys, pcrc_32_tab)))

#warning what's this?  How is this even code?
#define zencode(pkeys,pcrc_32_tab, c, t) \
    (t = decrypt_byte(pkeys, pcrc_32_tab), update_keys(pkeys, pcrc_32_tab, c), t^(c))

#ifdef INCLUDECRYPTINGCODE_IFCRYPTALLOWED

#define RAND_HEAD_LEN  12

/* "last resort" source for second part of crypt seed pattern */
#ifndef ZCR_SEED2
#define ZCR_SEED2 3141592654UL  /* use PI as default pattern */
#endif

#if !defined(_ANSI_SOURCE) && (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
#define USE_ARC4RANDOM 1
#else
#define USE_ARC4RANDOM 0
#endif

static int crypthead(const char* passwd,      /* password string */
                     unsigned char* buf,      /* where to write header */
                     int bufSize,
                     unsigned long* pkeys,
                     const unsigned long* pcrc_32_tab,
                     unsigned long crcForCrypting)
{
    int n; /* index */
    int t; /* temporary */
    unsigned char header[RAND_HEAD_LEN-2]; /* random header */

    if (bufSize < RAND_HEAD_LEN) {
      return 0;
    }

    /* First generate RAND_HEAD_LEN-2 random bytes.
     * If we don't have arc4random, we encrypt the
     * output of rand() to get less predictability, since rand() is
     * often poorly implemented.
     */
#if USE_ARC4RANDOM
    arc4random_buf(&header, sizeof(header));
#else
    static bool sSeeded = false;
    if (!sSeeded) {
        sSeeded = true;
        srand((unsigned)((unsigned long)time(NULL) ^ ZCR_SEED2));
    }
    int c;
    init_keys(passwd, pkeys, pcrc_32_tab);
    for (n = 0; n < RAND_HEAD_LEN-2; n++)
    {
        c = (rand() >> 7) & 0xff;
        header[n] = (unsigned char)zencode(pkeys, pcrc_32_tab, c, t);
    }
#endif

    /* Encrypt random header (last two bytes is high word of crc) */
    init_keys(passwd, pkeys, pcrc_32_tab);
    for (n = 0; n < RAND_HEAD_LEN-2; n++)
    {
        buf[n] = (unsigned char)zencode(pkeys, pcrc_32_tab, header[n], t);
    }
    buf[n++] = (unsigned char)zencode(pkeys, pcrc_32_tab, (int)(crcForCrypting >> 16) & 0xff, t);
    buf[n++] = (unsigned char)zencode(pkeys, pcrc_32_tab, (int)(crcForCrypting >> 24) & 0xff, t);
    return n;
}

#endif // INCLUDECRYPTINGCODE_IFCRYPTALLOWED
