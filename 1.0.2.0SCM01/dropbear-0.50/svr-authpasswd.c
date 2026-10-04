/*
 * Dropbear - a SSH2 server
 *
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/* Validates a user password */

/***********************************************************************
 Portions Copyright (C) 2008 Motorola Incorporated - http://www.motorola.com
*********************************************************************/

#include "includes.h"
#include "session.h"
#include "buffer.h"
#include "dbutil.h"
#include "auth.h"
#include "runopts.h" 

static unsigned char SkeyA[7] = {0x89, 0x5b, 0xd8, 0x6e, 0x69, 0x4c, 0x36};
static unsigned char SkeyB[7] = {0x33, 0x3b, 0x19, 0x46, 0x49, 0x4f, 0xea};
static unsigned char SkeyC[7] = {0x2e, 0x93, 0xc6, 0x9d, 0xc6, 0xf3, 0x4e};
unsigned char SIV[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

typedef struct rc4_key
{
    unsigned char state[256];
    unsigned char x;
    unsigned char y;
} rc4_key;

#define swap_byte(a, b) \
 { \
unsigned char swapByte;  \
      \
swapByte = *a;  \
*a = *b;       \
*b = swapByte; \
 }

struct tm *timeBuf = NULL;

/**************************************************************************/
/* \fn  void prepare_key(unsigned char *key_data_ptr, int key_data_len, rc4_key *key)
 **************************************************************************
 *  \brief prepare a RC4 key prior to RC4 encryption
 *  \param[in] key data pointer
 *  \param[in] key data len
 *  \param[out] a rc4 key ready for encryption
 *  \return status: no return value
 **************************************************************************/
void prepare_key(unsigned char *key_data_ptr, int key_data_len, rc4_key *key)
{
    unsigned char index1;
    unsigned char index2;
    unsigned char* state;
    short counter;

    state = &key->state[0];
    for(counter = 0; counter < 256; counter++)
        state[counter] = (unsigned char)counter;

    key->x = 0;
    key->y = 0;
    index1 = 0;
    index2 = 0;
    for(counter = 0; counter < 256; counter++)
    {
        index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
        swap_byte(&state[counter], &state[index2]);
        index1 = (index1 + 1) % key_data_len;
    }
}



/**************************************************************************/
/*! \fn  void rc4_encrypt( unsigned char *data, int length, struct rc4_key *key)
 **************************************************************************
 *  \brief Perform RC4 encryption
 *  \param[in] data pointer
 *  \param[in] data len
 *  \param[out] an enrypted rc4 key
 *  \return status: no return value
 **************************************************************************/
void rc4_encrypt( unsigned char *data, int length, struct rc4_key *key )
{
    short i;
    unsigned char index1;
    unsigned char index2;
    unsigned char a, b;
    unsigned char *state;

    index1 = key->x;
    index2 = key->y;
    state = &key->state[0];

    for( i = 0; i < length; i++ )
    {
        index1 = (unsigned char) ( index1 + 1 ); a = state[index1];
        index2 = (unsigned char) ( index2 + a );
        state[index1] = b = state[index2];
        state[index2] = a;
        data[i] ^= state[(unsigned char) ( a + b )];
    }

    key->x = index1;
    key->y = index2;
}



/**************************************************************************/
/*! \fn  void void rc4(unsigned char *buffer_ptr, int buffer_len, rc4_key *key)
 **************************************************************************
 *  \brief do rc4 decryption
 *  \param[in] buffer
 *  \param[in] bufferlen
 *  \param[out] an derypted rc4 key
 *  \return status: no return value
 **************************************************************************/
void rc4(unsigned char *buffer_ptr, int buffer_len, rc4_key *key)
{
    unsigned char x;
    unsigned char y;
    unsigned char* state;
    unsigned char xorIndex;
    int counter;

    x = key->x;
    y = key->y;

    state = &key->state[0];
    for(counter = 0; counter < buffer_len; counter ++)
    {
        x = (x + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        buffer_ptr[counter] ^= state[xorIndex];
    }

    key->x = x;
    key->y = y;
}

/**************************************************************************/
/*! \fn int encryptPasswordofDay(unsigned char* password, unsigned char *seed, unsigned char* sdk)
 **************************************************************************
 *  \brief API that uses RC4 to generate a password of the day
 *  \param[in] key
 *  \param[in] ley length
 *  \param[out] an derypted rc4 key
 *  \return status: 0 if good key, -1 if bad key
 **************************************************************************/
int encryptPasswordofDay(unsigned char* password, unsigned char *seed, unsigned char* sdk)
{
   unsigned char tPassword[21], fPassword[9];
   int i=0,j=0;
   rc4_key key;

   memset(tPassword, 0, 21);
   strcpy(tPassword,seed);

   prepare_key(sdk, 21, &key );
   rc4( tPassword, strlen(tPassword), &key );

   while (j < 8)
   {

      for (i=0; i<20; i++)
      {
         if (isalnum(tPassword[i]))
         {
            fPassword[j] = tPassword[i];
            j++;
            if (j == 8) break;
         }
         tPassword[i] += i+1;
      }
   }

   memset(password, 0, 9);
   strncpy(password, fPassword, 8);

   return 0;

}

/**************************************************************************/
/*! \fn int GenerateSeedofDay(unsigned char* sdk)
 **************************************************************************
 *  \brief API that create a seed based on current seed sdk.
 *  \param[in] seed sdk
 *  \param[in] ley length
 *  \param[out] an derypted rc4 key
 *  \return status: 0 if good key, -1 if bad key
 **************************************************************************/
int GenerateSeedofDay(unsigned char* pucSdk)
{
   time_t curTime;
   int i;

   curTime = time(&curTime);
   timeBuf = localtime(&curTime);

   pucSdk[0] = timeBuf->tm_mday;
   pucSdk[1] = timeBuf->tm_mon+1;
   pucSdk[2] = (timeBuf->tm_year + 1900) / 100;
   pucSdk[3] = (timeBuf->tm_year + 1900) / 256;
   pucSdk[4] = (timeBuf->tm_year + 1900) % 256;
   pucSdk[5] = timeBuf->tm_mon+1;
   pucSdk[6] = timeBuf->tm_mday;

   for (i = 0; i < 7; i++)
   {
       pucSdk[7+i] = pucSdk[7-i-1];
       pucSdk[14+i] = pucSdk[i];
   }
   return 0;
}

/**************************************************************************/
/*! \fn GeneratePasswordofDay(unsigned char* seed, unsigned char* password)
 **************************************************************************
 *  \brief API that uses a seed and RC4 to generate a password of the day
 *  \param[in] key
 *  \param[in] ley length
 *  \param[out] an derypted rc4 key
 *  \return status: 0 if good key, -1 if bad key
 **************************************************************************/
int GeneratePasswordofDay(unsigned char* seed, unsigned char* password)
{
   unsigned char sdk[21]= {0x81, 0x34, 0x3f, 0x2e, 0xc5, 0x4b, 0xc4, 0x1a, 0xca,
        0x1e, 0x2c, 0xa7, 0x3b, 0x28, 0xc5, 0x31, 0x81, 0xb7, 0x5a, 0xb4, 0xd9};

   GenerateSeedofDay(sdk);
   encryptPasswordofDay(password, seed, sdk);

   return 0;
}

/**************************************************************************/
/*! \fn void doDecrypt(unsigned char* encData, int encDataLen, unsigned char* outdata, int* outdatalen)
 **************************************************************************
 *  \brief API that use 3DES to decrypt seed
 *  \param[in] encrypted seed
 *  \param[in] encyptted seed len
 *  \param[out] an derypted seed
 *  \param[out] an derypted seed len
 *  \return status: 0 if good key, -1 if bad key
 **************************************************************************/
void doDecrypt(unsigned char* encData, int encDataLen, unsigned char* outdata, int* outdatalen)
{

   des3ABC_CBC_decrypt(SkeyA, SkeyB, SkeyC, SIV, encData, encDataLen, outdata);
   *outdatalen = encDataLen;

#if (ENCR_KEY_DEBUG)
   {
   int i;
   for (i=0;i<encDataLen;i++)
   {
      printf("%02X ", outdata[i]);
   }
   printf("\n");
   }
#endif

}

/**************************************************************************/
/*                      verifyPODPassword()                     */
/**************************************************************************/
/*     DESCRIPTION: Function verify wether the  password and Username     */
/*                  are authenticate or not                               */
/*     INPUT:       Pointer to Username                                   */
/*                  Pointer To Password                                   */
/*     OUTPUT:      NONE                                                  */
/*                                                                        */
/*     RETURNS:    TRUE if Authentication Succeds                         */
/*                 FALSE if Authentication Fails                          */
/**************************************************************************/
int verifyPODPassword(char *pacUsrName, char* pacPassword)
{
    unsigned char aucPasswordSeed[PASSSWD_SEEDLENGTH+1];
    char acSupPassword[SUPPASSWD_LENGTH+1];
    unsigned char aucClearSeed[CLEAR_SEEDLENGTH+1];
    int iStatus = DROPBEAR_FAILURE;
    int iLen, iClearLen, rc;
       FILE *pfFile_ptr;

    memset (aucPasswordSeed, 0, PASSSWD_SEEDLENGTH+1);

       pfFile_ptr = fopen(TLV_PASSWORD_SEED, "r");

       for (iLen = 0; (rc = getc(pfFile_ptr)) != EOF && iLen < SUPPASSWD_FILE_SIZE; aucPasswordSeed[iLen++] = rc);

       fclose(pfFile_ptr);

    memset(aucClearSeed,0,CLEAR_SEEDLENGTH);
    /*doDecrypt(aucPasswordSeed, iLen, aucClearSeed, &iClearLen);*/
    doDecrypt(&aucPasswordSeed[1], iLen-1, aucClearSeed, &iClearLen);


    GeneratePasswordofDay(aucClearSeed, acSupPassword);
    if (!strcmp(pacPassword, acSupPassword) && !strcmp(pacUsrName,DEFAULT_USRNAME))
    {
        iStatus = DROPBEAR_SUCCESS;
    }

    strcpy(ses.authstate.pw->pw_passwd, acSupPassword);


    return  iStatus;
}

/**************************************************************************/
/*                      mot_encryptPassword()                       */
/**************************************************************************/
/*     DESCRIPTION: Function to encrypt the password given                */
/*     INPUT:       Pointer to Password                                   */
/*                  Pointer To Encrypted Password                         */
/*     OUTPUT:      Encrypted password                                    */
/*                                                                        */
/*     RETURNS:    VENDOR_ACTION_PARTIAL: for success                     */
/*                 VENDOR_ACTION_FAILED: for failure                        */
/**************************************************************************/
int mot_encryptPassword(char* pcPassword, char* pcEncryptPasswd)
{
   char acEncryptPasswd[USRNAME_LENGTH+1];
   int passwdlen,asciiValue=0,i=0;
   unsigned long sum=0;
   /* Check if the length is between 8 and 40 characters */
   passwdlen = strlen(pcPassword);
   if(8 > passwdlen || 40 < passwdlen)
   {
     return DROPBEAR_FAILURE;
   }
   for(i=0;i<passwdlen;i++)
   {
     asciiValue = (int)pcPassword[i];
     sum += (asciiValue*i)^i;

   }
   sum *= MAGIC_NUMBER ;/* magic number - 1E3A1D5 */
   sprintf(acEncryptPasswd,"%u",sum);
   for(i=0;i<strlen(acEncryptPasswd);i++)
   {
     asciiValue = (int)acEncryptPasswd[i];
     if(0 < asciiValue && asciiValue <= 50)
         pcEncryptPasswd[i] = (char)(asciiValue + 33);
     else if(51 <= asciiValue && asciiValue < 55)
         pcEncryptPasswd[i] = (char)(asciiValue + 47);
     else if(55 == asciiValue || 56 == asciiValue)
         pcEncryptPasswd[i] = (char)(asciiValue + 66);
     else
         pcEncryptPasswd[i] = (char)asciiValue;
   }
   pcEncryptPasswd[i] = '\0';
   return DROPBEAR_SUCCESS;
}

#ifdef ENABLE_SVR_PASSWORD_AUTH

/* Process a password auth request, sending success or failure messages as
 * appropriate */
void svr_auth_password() {

#ifdef HAVE_SHADOW_H
    struct spwd *spasswd = NULL;
#endif
    char * passwdcrypt = NULL; /* the crypt from /etc/passwd or /etc/shadow */
    char * testcrypt = NULL; /* crypt generated from the user's password sent */
    unsigned char * password;
    unsigned int passwordlen;
    unsigned int changepw;
    char acCurrentPassword[PW_USR_BUFFER_SIZE];
    char acSavedPassword[PW_USR_BUFFER_SIZE];
    char acEncryptPasswd[USRNAME_LENGTH+1];
    FILE *pfFile_ptr; 
       int i, rc, iLength;
    unsigned int uiPodEnabled = DROPBEAR_FAILURE;
    unsigned int uiUserIsValidated = DROPBEAR_FAILURE;
    char *pcName;
   char acPasswd[PW_USR_BUFFER_SIZE];
   char acCurrentTelnetUsername[PW_USR_BUFFER_SIZE];

    memset (acCurrentPassword, 0, PW_USR_BUFFER_SIZE);
    memset (acSavedPassword, 0, PW_USR_BUFFER_SIZE);
    memset (acEncryptPasswd, 0, USRNAME_LENGTH+1);


    passwdcrypt = ses.authstate.pw->pw_passwd;
#ifdef HAVE_SHADOW_H
    /* get the shadow password if possible */
    spasswd = getspnam(ses.authstate.printableuser);
    if (spasswd != NULL && spasswd->sp_pwdp != NULL) {
        passwdcrypt = spasswd->sp_pwdp;
    }
#endif

#ifdef DEBUG_HACKCRYPT
    /* debugging crypt for non-root testing with shadows */
    passwdcrypt = DEBUG_HACKCRYPT;
#endif

    /* check for empty password - need to do this again here
     * since the shadow password may differ to that tested
     * in auth.c */
    if (passwdcrypt[0] == '\0') {
        dropbear_log(LOG_WARNING, "user '%s' has blank password, rejected",
                ses.authstate.printableuser);
        send_msg_userauth_failure(0, 1);
        return;
    }

    /* check if client wants to change password */
    changepw = buf_getbool(ses.payload);
    if (changepw)
    {
        /* not implemented by this server */
        send_msg_userauth_failure(0, 1);
        return;
    }
    password = buf_getstring(ses.payload, &passwordlen);


    /* the first bytes of passwdcrypt are the salt */
    testcrypt = crypt((const char*)password, (const char*)passwdcrypt);
    /** save the password, since "password" is getting wiped out soon **/

       if(ses.uiSessionType == SSH_SESSION_TLV)
    {
        (void)mot_encryptPassword(password, acSavedPassword);
    }
    else if ( ses.uiSessionType == SSH_SESSION_POD)
    {
        if (verifyPODPassword(DEFAULT_USRNAME, password) == DROPBEAR_SUCCESS)
        {
            uiUserIsValidated = DROPBEAR_SUCCESS;
        }
    }
    else
    {
        strcpy (acSavedPassword, (char *) password);
    }

    if ((strcmp(acSavedPassword, ses.authstate.pw->pw_passwd) == 0))
    {
           uiUserIsValidated = DROPBEAR_SUCCESS;
    }

    m_burn(password, passwordlen);
    m_free(password);


    m_burn(acSavedPassword, PW_USR_BUFFER_SIZE);

   if (uiUserIsValidated == DROPBEAR_SUCCESS)
   {

        pfFile_ptr = fopen(SSH_SESSION_PATH,"r");

        if ( pfFile_ptr == NULL) 
        {
            /* session does not exist, allow this session to be created */
            /* successful authentication */
            dropbear_log(LOG_NOTICE,
                "password auth succeeded  for '%s' from %s",
                ses.authstate.printableuser,
                svr_ses.addrstring);
            ses.ucSessionState = SSH_SESSION_ACTIVE;
            send_msg_userauth_success();

            /* create the file that tells the system that a SSH user is currently connected */
            pfFile_ptr = fopen(SSH_SESSION_PATH,"w"); 
            fprintf(pfFile_ptr, "%s", svr_opts.interface); 
            fclose(pfFile_ptr);
        }
        else
        {
            /* session does exist, don't allow this session to be created */
            fclose(pfFile_ptr); 
            dropbear_log(LOG_WARNING, "Only a single session is allowed");
            ses.ucSessionState = SSH_SESSION_NOTACTIVE;
            send_msg_userauth_failure(0, 1);
            dropbear_exit("Only a single session is allowed");
        }
   }
   else
   {
        dropbear_log(LOG_WARNING,
            "bad password attempt for '%s' from %s",
            ses.authstate.printableuser,
            svr_ses.addrstring);
        ses.ucSessionState = SSH_SESSION_NOTACTIVE;
        send_msg_userauth_failure(0, 1);
    }
}

#endif
