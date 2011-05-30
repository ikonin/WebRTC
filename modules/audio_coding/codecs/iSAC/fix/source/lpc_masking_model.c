/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * lpc_masking_model.c
 *
 * LPC analysis and filtering functions
 *
 */

#include "codec.h"
#include "entropy_coding.h"
#include "settings.h"


/* The conversion is implemented by the step-down algorithm */
void WebRtcSpl_AToK_JSK(
    WebRtc_Word16 *a16, /* Q11 */
    WebRtc_Word16 useOrder,
    WebRtc_Word16 *k16  /* Q15 */
                        )
{
  int m, k;
  WebRtc_Word32 tmp32[MAX_AR_MODEL_ORDER];
  WebRtc_Word32 tmp32b;
  WebRtc_Word32 tmp_inv_denum32;
  WebRtc_Word16 tmp_inv_denum16;

  k16[useOrder-1]= WEBRTC_SPL_LSHIFT_W16(a16[useOrder], 4); //Q11<<4 => Q15

  for (m=useOrder-1; m>0; m--) {
    tmp_inv_denum32 = ((WebRtc_Word32) 1073741823) - WEBRTC_SPL_MUL_16_16(k16[m], k16[m]); // (1 - k^2) in Q30
    tmp_inv_denum16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp_inv_denum32, 15); // (1 - k^2) in Q15

    for (k=1; k<=m; k++) {
      tmp32b = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)a16[k], 16) -
          WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16(k16[m], a16[m-k+1]), 1);

      tmp32[k] = WebRtcSpl_DivW32W16(tmp32b, tmp_inv_denum16); //Q27/Q15 = Q12
    }

    for (k=1; k<m; k++) {
      a16[k] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32[k], 1); //Q12>>1 => Q11
    }

    tmp32[m] = WEBRTC_SPL_SAT(4092, tmp32[m], -4092);
    k16[m-1] = (WebRtc_Word16) WEBRTC_SPL_LSHIFT_W32(tmp32[m], 3); //Q12<<3 => Q15
  }

  return;
}





WebRtc_Word16 WebRtcSpl_LevinsonW32_JSK(
    WebRtc_Word32 *R,  /* (i) Autocorrelation of length >= order+1 */
    WebRtc_Word16 *A,  /* (o) A[0..order] LPC coefficients (Q11) */
    WebRtc_Word16 *K,  /* (o) K[0...order-1] Reflection coefficients (Q15) */
    WebRtc_Word16 order /* (i) filter order */
                                        ) {
  WebRtc_Word16 i, j;
  WebRtc_Word16 R_hi[LEVINSON_MAX_ORDER+1], R_low[LEVINSON_MAX_ORDER+1];
  /* Aurocorr coefficients in high precision */
  WebRtc_Word16 A_hi[LEVINSON_MAX_ORDER+1], A_low[LEVINSON_MAX_ORDER+1];
  /* LPC coefficients in high precicion */
  WebRtc_Word16 A_upd_hi[LEVINSON_MAX_ORDER+1], A_upd_low[LEVINSON_MAX_ORDER+1];
  /* LPC coefficients for next iteration */
  WebRtc_Word16 K_hi, K_low;      /* reflection coefficient in high precision */
  WebRtc_Word16 Alpha_hi, Alpha_low, Alpha_exp; /* Prediction gain Alpha in high precision
                                                   and with scale factor */
  WebRtc_Word16 tmp_hi, tmp_low;
  WebRtc_Word32 temp1W32, temp2W32, temp3W32;
  WebRtc_Word16 norm;

  /* Normalize the autocorrelation R[0]...R[order+1] */

  norm = WebRtcSpl_NormW32(R[0]);

  for (i=order;i>=0;i--) {
    temp1W32 = WEBRTC_SPL_LSHIFT_W32(R[i], norm);
    /* Put R in hi and low format */
    R_hi[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
    R_low[i] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)R_hi[i], 16)), 1);
  }

  /* K = A[1] = -R[1] / R[0] */

  temp2W32  = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)R_hi[1],16) +
      WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)R_low[1],1);     /* R[1] in Q31      */
  temp3W32  = WEBRTC_SPL_ABS_W32(temp2W32);      /* abs R[1]         */
  temp1W32  = WebRtcSpl_DivW32HiLow(temp3W32, R_hi[0], R_low[0]); /* abs(R[1])/R[0] in Q31 */
  /* Put back the sign on R[1] */
  if (temp2W32 > 0) {
    temp1W32 = -temp1W32;
  }

  /* Put K in hi and low format */
  K_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
  K_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)K_hi, 16)), 1);

  /* Store first reflection coefficient */
  K[0] = K_hi;

  temp1W32 = WEBRTC_SPL_RSHIFT_W32(temp1W32, 4);    /* A[1] in Q27      */

  /* Put A[1] in hi and low format */
  A_hi[1] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
  A_low[1] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_hi[1], 16)), 1);

  /*  Alpha = R[0] * (1-K^2) */

  temp1W32  = WEBRTC_SPL_LSHIFT_W32((WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(K_hi, K_low), 14) +
                                      WEBRTC_SPL_MUL_16_16(K_hi, K_hi)), 1); /* temp1W32 = k^2 in Q31 */

  temp1W32 = WEBRTC_SPL_ABS_W32(temp1W32);    /* Guard against <0 */
  temp1W32 = (WebRtc_Word32)0x7fffffffL - temp1W32;    /* temp1W32 = (1 - K[0]*K[0]) in Q31 */

  /* Store temp1W32 = 1 - K[0]*K[0] on hi and low format */
  tmp_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
  tmp_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)tmp_hi, 16)), 1);

  /* Calculate Alpha in Q31 */
  temp1W32 = WEBRTC_SPL_LSHIFT_W32((WEBRTC_SPL_MUL_16_16(R_hi[0], tmp_hi) +
                                     WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(R_hi[0], tmp_low), 15) +
                                     WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(R_low[0], tmp_hi), 15) ), 1);

  /* Normalize Alpha and put it in hi and low format */

  Alpha_exp = WebRtcSpl_NormW32(temp1W32);
  temp1W32 = WEBRTC_SPL_LSHIFT_W32(temp1W32, Alpha_exp);
  Alpha_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
  Alpha_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)Alpha_hi, 16)), 1);

  /* Perform the iterative calculations in the
     Levinson Durbin algorithm */

  for (i=2; i<=order; i++)
  {

    /*                    ----
                          \
                          temp1W32 =  R[i] + > R[j]*A[i-j]
                          /
                          ----
                          j=1..i-1
    */

    temp1W32 = 0;

    for(j=1; j<i; j++) {
      /* temp1W32 is in Q31 */
      temp1W32 += (WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16(R_hi[j], A_hi[i-j]), 1) +
                   WEBRTC_SPL_LSHIFT_W32(( WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(R_hi[j], A_low[i-j]), 15) +
                                            WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(R_low[j], A_hi[i-j]), 15) ), 1));
    }

    temp1W32  = WEBRTC_SPL_LSHIFT_W32(temp1W32, 4);
    temp1W32 += (WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)R_hi[i], 16) +
                 WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)R_low[i], 1));

    /* K = -temp1W32 / Alpha */
    temp2W32 = WEBRTC_SPL_ABS_W32(temp1W32);      /* abs(temp1W32) */
    temp3W32 = WebRtcSpl_DivW32HiLow(temp2W32, Alpha_hi, Alpha_low); /* abs(temp1W32)/Alpha */

    /* Put the sign of temp1W32 back again */
    if (temp1W32 > 0) {
      temp3W32 = -temp3W32;
    }

    /* Use the Alpha shifts from earlier to denormalize */
    norm = WebRtcSpl_NormW32(temp3W32);
    if ((Alpha_exp <= norm)||(temp3W32==0)) {
      temp3W32 = WEBRTC_SPL_LSHIFT_W32(temp3W32, Alpha_exp);
    } else {
      if (temp3W32 > 0)
      {
        temp3W32 = (WebRtc_Word32)0x7fffffffL;
      } else
      {
        temp3W32 = (WebRtc_Word32)0x80000000L;
      }
    }

    /* Put K on hi and low format */
    K_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp3W32, 16);
    K_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp3W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)K_hi, 16)), 1);

    /* Store Reflection coefficient in Q15 */
    K[i-1] = K_hi;

    /* Test for unstable filter. If unstable return 0 and let the
       user decide what to do in that case
    */

    if ((WebRtc_Word32)WEBRTC_SPL_ABS_W16(K_hi) > (WebRtc_Word32)32740) {
      return(-i); /* Unstable filter */
    }

    /*
      Compute updated LPC coefficient: Anew[i]
      Anew[j]= A[j] + K*A[i-j]   for j=1..i-1
      Anew[i]= K
    */

    for(j=1; j<i; j++)
    {
      temp1W32  = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_hi[j],16) +
          WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_low[j],1);    /* temp1W32 = A[j] in Q27 */

      temp1W32 += WEBRTC_SPL_LSHIFT_W32(( WEBRTC_SPL_MUL_16_16(K_hi, A_hi[i-j]) +
                                           WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(K_hi, A_low[i-j]), 15) +
                                           WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(K_low, A_hi[i-j]), 15) ), 1); /* temp1W32 += K*A[i-j] in Q27 */

      /* Put Anew in hi and low format */
      A_upd_hi[j] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
      A_upd_low[j] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_upd_hi[j], 16)), 1);
    }

    temp3W32 = WEBRTC_SPL_RSHIFT_W32(temp3W32, 4);     /* temp3W32 = K in Q27 (Convert from Q31 to Q27) */

    /* Store Anew in hi and low format */
    A_upd_hi[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp3W32, 16);
    A_upd_low[i] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp3W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_upd_hi[i], 16)), 1);

    /*  Alpha = Alpha * (1-K^2) */

    temp1W32  = WEBRTC_SPL_LSHIFT_W32((WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(K_hi, K_low), 14) +
                                        WEBRTC_SPL_MUL_16_16(K_hi, K_hi)), 1);  /* K*K in Q31 */

    temp1W32 = WEBRTC_SPL_ABS_W32(temp1W32);      /* Guard against <0 */
    temp1W32 = (WebRtc_Word32)0x7fffffffL - temp1W32;      /* 1 - K*K  in Q31 */

    /* Convert 1- K^2 in hi and low format */
    tmp_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
    tmp_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)tmp_hi, 16)), 1);

    /* Calculate Alpha = Alpha * (1-K^2) in Q31 */
    temp1W32 = WEBRTC_SPL_LSHIFT_W32(( WEBRTC_SPL_MUL_16_16(Alpha_hi, tmp_hi) +
                                        WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(Alpha_hi, tmp_low), 15) +
                                        WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_16(Alpha_low, tmp_hi), 15)), 1);

    /* Normalize Alpha and store it on hi and low format */

    norm = WebRtcSpl_NormW32(temp1W32);
    temp1W32 = WEBRTC_SPL_LSHIFT_W32(temp1W32, norm);

    Alpha_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(temp1W32, 16);
    Alpha_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp1W32 - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)Alpha_hi, 16)), 1);

    /* Update the total nomalization of Alpha */
    Alpha_exp = Alpha_exp + norm;

    /* Update A[] */

    for(j=1; j<=i; j++)
    {
      A_hi[j] =A_upd_hi[j];
      A_low[j] =A_upd_low[j];
    }
  }

  /*
    Set A[0] to 1.0 and store the A[i] i=1...order in Q12
    (Convert from Q27 and use rounding)
  */

  A[0] = 2048;

  for(i=1; i<=order; i++) {
    /* temp1W32 in Q27 */
    temp1W32 = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_hi[i], 16) +
        WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)A_low[i], 1);
    /* Round and store upper word */
    A[i] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(temp1W32+(WebRtc_Word32)32768, 16);
  }
  return(1); /* Stable filters */
}





/* window */
/* Matlab generation of floating point code:
 *  t = (1:256)/257; r = 1-(1-t).^.45; w = sin(r*pi).^3; w = w/sum(w); plot((1:256)/8, w); grid;
 *  for k=1:16, fprintf(1, '%.8f, ', w(k*16 + (-15:0))); fprintf(1, '\n'); end
 * All values are multiplyed with 2^21 in fixed point code.
 */
static const WebRtc_Word16 kWindowAutocorr[WINLEN] = {
  0,     0,     0,     0,     0,     1,     1,     2,     2,     3,     5,     6,
  8,    10,    12,    14,    17,    20,    24,    28,    33,    38,    43,    49,
  56,    63,    71,    79,    88,    98,   108,   119,   131,   143,   157,   171,
  186,   202,   219,   237,   256,   275,   296,   318,   341,   365,   390,   416,
  444,   472,   502,   533,   566,   600,   635,   671,   709,   748,   789,   831,
  875,   920,   967,  1015,  1065,  1116,  1170,  1224,  1281,  1339,  1399,  1461,
  1525,  1590,  1657,  1726,  1797,  1870,  1945,  2021,  2100,  2181,  2263,  2348,
  2434,  2523,  2614,  2706,  2801,  2898,  2997,  3099,  3202,  3307,  3415,  3525,
  3637,  3751,  3867,  3986,  4106,  4229,  4354,  4481,  4611,  4742,  4876,  5012,
  5150,  5291,  5433,  5578,  5725,  5874,  6025,  6178,  6333,  6490,  6650,  6811,
  6974,  7140,  7307,  7476,  7647,  7820,  7995,  8171,  8349,  8529,  8711,  8894,
  9079,  9265,  9453,  9642,  9833, 10024, 10217, 10412, 10607, 10803, 11000, 11199,
  11398, 11597, 11797, 11998, 12200, 12401, 12603, 12805, 13008, 13210, 13412, 13614,
  13815, 14016, 14216, 14416, 14615, 14813, 15009, 15205, 15399, 15591, 15782, 15971,
  16157, 16342, 16524, 16704, 16881, 17056, 17227, 17395, 17559, 17720, 17877, 18030,
  18179, 18323, 18462, 18597, 18727, 18851, 18970, 19082, 19189, 19290, 19384, 19471,
  19551, 19623, 19689, 19746, 19795, 19835, 19867, 19890, 19904, 19908, 19902, 19886,
  19860, 19823, 19775, 19715, 19644, 19561, 19465, 19357, 19237, 19102, 18955, 18793,
  18618, 18428, 18223, 18004, 17769, 17518, 17252, 16970, 16672, 16357, 16025, 15677,
  15311, 14929, 14529, 14111, 13677, 13225, 12755, 12268, 11764, 11243, 10706, 10152,
  9583,  8998,  8399,  7787,  7162,  6527,  5883,  5231,  4576,  3919,  3265,  2620,
  1990,  1386,   825,   333
};


/* By using a hearing threshold level in dB of -28 dB (higher value gives more noise),
   the H_T_H (in float) can be calculated as:
   H_T_H = pow(10.0, 0.05 * (-28.0)) = 0.039810717055350
   In Q19, H_T_H becomes round(0.039810717055350*2^19) ~= 20872, i.e.
   H_T_H = 20872/524288.0, and H_T_HQ19 = 20872;
*/


/* The bandwidth expansion vectors are created from:
   kPolyVecLo=[0.900000,0.810000,0.729000,0.656100,0.590490,0.531441,0.478297,0.430467,0.387420,0.348678,0.313811,0.282430];
   kPolyVecHi=[0.800000,0.640000,0.512000,0.409600,0.327680,0.262144];
   round(kPolyVecLo*32768)
   round(kPolyVecHi*32768)
*/
static const WebRtc_Word16 kPolyVecLo[12] = {
  29491, 26542, 23888, 21499, 19349, 17414, 15673, 14106, 12695, 11425, 10283, 9255
};
static const WebRtc_Word16 kPolyVecHi[6] = {
  26214, 20972, 16777, 13422, 10737, 8590
};

static __inline WebRtc_Word32 log2_Q8_LPC( WebRtc_UWord32 x ) {

  WebRtc_Word32 zeros, lg2;
  WebRtc_Word16 frac;

  zeros=WebRtcSpl_NormU32(x);
  frac=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(((WebRtc_UWord32)WEBRTC_SPL_LSHIFT_W32(x, zeros)&0x7FFFFFFF), 23);

  /* log2(x) */

  lg2= (WEBRTC_SPL_LSHIFT_W16((31-zeros), 8)+frac);
  return lg2;

}

static const WebRtc_Word16 kMulPitchGain = -25; /* 200/256 in Q5 */
static const WebRtc_Word16 kChngFactor = 3523; /* log10(2)*10/4*0.4/1.4=log10(2)/1.4= 0.2150 in Q14 */
static const WebRtc_Word16 kExp2 = 11819; /* 1/log(2) */

void WebRtcIsacfix_GetVars(const WebRtc_Word16 *input, const WebRtc_Word16 *pitchGains_Q12,
                           WebRtc_UWord32 *oldEnergy, WebRtc_Word16 *varscale)
{
  int k;
  WebRtc_UWord32 nrgQ[4];
  WebRtc_Word16 nrgQlog[4];
  WebRtc_Word16 tmp16, chng1, chng2, chng3, chng4, tmp, chngQ, oldNrgQlog, pgQ, pg3;
  WebRtc_Word32 expPg32;
  WebRtc_Word16 expPg, divVal;
  WebRtc_Word16 tmp16_1, tmp16_2;

  /* Calculate energies of first and second frame halfs */
  nrgQ[0]=0;
  for (k = QLOOKAHEAD/2; k < (FRAMESAMPLES/4 + QLOOKAHEAD) / 2; k++) {
    nrgQ[0] +=WEBRTC_SPL_MUL_16_16(input[k],input[k]);
  }
  nrgQ[1]=0;
  for ( ; k < (FRAMESAMPLES/2 + QLOOKAHEAD) / 2; k++) {
    nrgQ[1] +=WEBRTC_SPL_MUL_16_16(input[k],input[k]);
  }
  nrgQ[2]=0;
  for ( ; k < (WEBRTC_SPL_MUL_16_16(FRAMESAMPLES, 3)/4 + QLOOKAHEAD) / 2; k++) {
    nrgQ[2] +=WEBRTC_SPL_MUL_16_16(input[k],input[k]);
  }
  nrgQ[3]=0;
  for ( ; k < (FRAMESAMPLES + QLOOKAHEAD) / 2; k++) {
    nrgQ[3] +=WEBRTC_SPL_MUL_16_16(input[k],input[k]);
  }

  for ( k=0; k<4; k++) {
    nrgQlog[k] = (WebRtc_Word16)log2_Q8_LPC(nrgQ[k]); /* log2(nrgQ) */
  }
  oldNrgQlog = (WebRtc_Word16)log2_Q8_LPC(*oldEnergy);

  /* Calculate average level change */
  chng1 = WEBRTC_SPL_ABS_W16(nrgQlog[3]-nrgQlog[2]);
  chng2 = WEBRTC_SPL_ABS_W16(nrgQlog[2]-nrgQlog[1]);
  chng3 = WEBRTC_SPL_ABS_W16(nrgQlog[1]-nrgQlog[0]);
  chng4 = WEBRTC_SPL_ABS_W16(nrgQlog[0]-oldNrgQlog);
  tmp = chng1+chng2+chng3+chng4;
  chngQ = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp, kChngFactor, 10); /* Q12 */
  chngQ += 2926; /* + 1.0/1.4 in Q12 */

  /* Find average pitch gain */
  pgQ = 0;
  for (k=0; k<4; k++)
  {
    pgQ += pitchGains_Q12[k];
  }

  pg3 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(pgQ, pgQ,11); /* pgQ in Q(12+2)=Q14. Q14*Q14>>11 => Q17 */
  pg3 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(pgQ, pg3,13); /* Q17*Q14>>13 =>Q18  */
  pg3 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(pg3, kMulPitchGain ,5); /* Q10  kMulPitchGain = -25 = -200 in Q-3. */

  tmp16=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(kExp2,pg3,13);/* Q13*Q10>>13 => Q10*/
  if (tmp16<0) {
    tmp16_2 = (0x0400 | (tmp16 & 0x03FF));
    tmp16_1 = (WEBRTC_SPL_RSHIFT_W16((WebRtc_UWord16)(tmp16 ^ 0xFFFF), 10)-3); /* Gives result in Q14 */
    if (tmp16_1<0)
      expPg=(WebRtc_Word16) -WEBRTC_SPL_LSHIFT_W16(tmp16_2, -tmp16_1);
    else
      expPg=(WebRtc_Word16) -WEBRTC_SPL_RSHIFT_W16(tmp16_2, tmp16_1);
  } else
    expPg = (WebRtc_Word16) -16384; /* 1 in Q14, since 2^0=1 */

  expPg32 = (WebRtc_Word32)WEBRTC_SPL_LSHIFT_W16((WebRtc_Word32)expPg, 8); /* Q22 */
  divVal = WebRtcSpl_DivW32W16ResW16(expPg32, chngQ); /* Q22/Q12=Q10 */

  tmp16=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(kExp2,divVal,13);/* Q13*Q10>>13 => Q10*/
  if (tmp16<0) {
    tmp16_2 = (0x0400 | (tmp16 & 0x03FF));
    tmp16_1 = (WEBRTC_SPL_RSHIFT_W16((WebRtc_UWord16)(tmp16 ^ 0xFFFF), 10)-3); /* Gives result in Q14 */
    if (tmp16_1<0)
      expPg=(WebRtc_Word16) WEBRTC_SPL_LSHIFT_W16(tmp16_2, -tmp16_1);
    else
      expPg=(WebRtc_Word16) WEBRTC_SPL_RSHIFT_W16(tmp16_2, tmp16_1);
  } else
    expPg = (WebRtc_Word16) 16384; /* 1 in Q14, since 2^0=1 */

  *varscale = expPg-1;
  *oldEnergy = nrgQ[3];
}



static __inline WebRtc_Word16  exp2_Q10_T(WebRtc_Word16 x) { // Both in and out in Q10

  WebRtc_Word16 tmp16_1, tmp16_2;

  tmp16_2=(WebRtc_Word16)(0x0400|(x&0x03FF));
  tmp16_1=-(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(x,10);
  if(tmp16_1>0)
    return (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W16(tmp16_2, tmp16_1);
  else
    return (WebRtc_Word16) WEBRTC_SPL_LSHIFT_W16(tmp16_2, -tmp16_1);

}




void WebRtcIsacfix_GetLpcCoef(WebRtc_Word16 *inLoQ0,
                              WebRtc_Word16 *inHiQ0,
                              MaskFiltstr_enc *maskdata,
                              WebRtc_Word16 snrQ10,
                              const WebRtc_Word16 *pitchGains_Q12,
                              WebRtc_Word32 *gain_lo_hiQ17,
                              WebRtc_Word16 *lo_coeffQ15,
                              WebRtc_Word16 *hi_coeffQ15)
{
  int k, n, j, ii;
  WebRtc_Word16 pos1, pos2;
  WebRtc_Word16 sh_lo, sh_hi, sh, ssh, shMem;
  WebRtc_Word16 varscaleQ14;

  WebRtc_Word16 tmpQQlo, tmpQQhi;
  WebRtc_Word32 tmp32;
  WebRtc_Word16 tmp16,tmp16b;

  WebRtc_Word16 polyHI[ORDERHI+1];
  WebRtc_Word16 rcQ15_lo[ORDERLO], rcQ15_hi[ORDERHI];


  WebRtc_Word16 DataLoQ6[WINLEN], DataHiQ6[WINLEN];
  WebRtc_Word32 corrloQQ[ORDERLO+2];
  WebRtc_Word32 corrhiQQ[ORDERHI+1];
  WebRtc_Word32 corrlo2QQ[ORDERLO+1];
  WebRtc_Word16 scale;
  WebRtc_Word16 QdomLO, QdomHI, newQdomHI, newQdomLO;

  WebRtc_Word32 round;
  WebRtc_Word32 res_nrgQQ;
  WebRtc_Word32 sqrt_nrg;

  WebRtc_Word32 aSQR32;

  /* less-noise-at-low-frequencies factor */
  WebRtc_Word16 aaQ14;

  /* Multiplication with 1/sqrt(12) ~= 0.28901734104046 can be done by convertion to
     Q15, i.e. round(0.28901734104046*32768) = 9471, and use 9471/32768.0 ~= 0.289032
  */
  WebRtc_Word16 snrq, shft;

  WebRtc_Word16 tmp16a;
  WebRtc_Word32 tmp32a, tmp32b, tmp32c;

  WebRtc_Word16 a_LOQ11[ORDERLO+1];
  WebRtc_Word16 k_vecloQ15[ORDERLO];
  WebRtc_Word16 a_HIQ12[ORDERHI+1];
  WebRtc_Word16 k_vechiQ15[ORDERHI];

  WebRtc_Word16 stab;

  snrq=snrQ10;

  /* SNR= C * 2 ^ (D * snrq) ; C=0.289, D=0.05*log2(10)=0.166 (~=172 in Q10)*/
  tmp16 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(snrq, 172, 10); // Q10
  tmp16b = exp2_Q10_T(tmp16); // Q10
  snrq = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(tmp16b, 285, 10); // Q10

  /* change quallevel depending on pitch gains and level fluctuations */
  WebRtcIsacfix_GetVars(inLoQ0, pitchGains_Q12, &(maskdata->OldEnergy), &varscaleQ14);

  /* less-noise-at-low-frequencies factor */
  /* Calculation of 0.35 * (0.5 + 0.5 * varscale) in fixpoint:
     With 0.35 in Q16 (0.35 ~= 22938/65536.0 = 0.3500061) and varscaleQ14 in Q14,
     we get Q16*Q14>>16 = Q14
  */
  aaQ14 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
      (WEBRTC_SPL_MUL_16_16(22938, (8192 + WEBRTC_SPL_RSHIFT_W32(varscaleQ14, 1)))
       + ((WebRtc_Word32)32768)), 16);

  /* Calculate tmp = (1.0 + aa*aa); in Q12 */
  tmp16 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(aaQ14, aaQ14, 15); //Q14*Q14>>15 = Q13
  tmpQQlo = 4096 + WEBRTC_SPL_RSHIFT_W16(tmp16, 1); // Q12 + Q13>>1 = Q12

  /* Calculate tmp = (1.0+aa) * (1.0+aa); */
  tmp16 = 8192 + WEBRTC_SPL_RSHIFT_W16(aaQ14, 1); // 1+a in Q13
  tmpQQhi = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(tmp16, tmp16, 14); //Q13*Q13>>14 = Q12

  /* replace data in buffer by new look-ahead data */
  for (pos1 = 0; pos1 < QLOOKAHEAD; pos1++) {
    maskdata->DataBufferLoQ0[pos1 + WINLEN - QLOOKAHEAD] = inLoQ0[pos1];
  }

  for (k = 0; k < SUBFRAMES; k++) {

    /* Update input buffer and multiply signal with window */
    for (pos1 = 0; pos1 < WINLEN - UPDATE/2; pos1++) {
      maskdata->DataBufferLoQ0[pos1] = maskdata->DataBufferLoQ0[pos1 + UPDATE/2];
      maskdata->DataBufferHiQ0[pos1] = maskdata->DataBufferHiQ0[pos1 + UPDATE/2];
      DataLoQ6[pos1] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(
          maskdata->DataBufferLoQ0[pos1], kWindowAutocorr[pos1], 15); // Q0*Q21>>15 = Q6
      DataHiQ6[pos1] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(
          maskdata->DataBufferHiQ0[pos1], kWindowAutocorr[pos1], 15); // Q0*Q21>>15 = Q6
    }
    pos2 = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16(k, UPDATE)/2);
    for (n = 0; n < UPDATE/2; n++, pos1++) {
      maskdata->DataBufferLoQ0[pos1] = inLoQ0[QLOOKAHEAD + pos2];
      maskdata->DataBufferHiQ0[pos1] = inHiQ0[pos2++];
      DataLoQ6[pos1] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(
          maskdata->DataBufferLoQ0[pos1], kWindowAutocorr[pos1], 15); // Q0*Q21>>15 = Q6
      DataHiQ6[pos1] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(
          maskdata->DataBufferHiQ0[pos1], kWindowAutocorr[pos1], 15); // Q0*Q21>>15 = Q6
    }

    /* Get correlation coefficients */
    /* The highest absolute value measured inside DataLo in the test set
       For DataHi, corresponding value was 160.

       This means that it should be possible to represent the input values
       to WebRtcSpl_AutoCorrelation() as Q6 values (since 307*2^6 =
       19648). Of course, Q0 will also work, but due to the low energy in
       DataLo and DataHi, the outputted autocorrelation will be more accurate
       and mimic the floating point code better, by being in an high as possible
       Q-domain.
    */

    WebRtcIsacfix_AutocorrFix(corrloQQ,DataLoQ6,WINLEN, ORDERLO+1, &scale);
    QdomLO = 12-scale; // QdomLO is the Q-domain of corrloQQ
    sh_lo = WebRtcSpl_NormW32(corrloQQ[0]);
    QdomLO += sh_lo;
    for (ii=0; ii<ORDERLO+2; ii++) {
      corrloQQ[ii] = WEBRTC_SPL_LSHIFT_W32(corrloQQ[ii], sh_lo);
    }
    /* It is investigated whether it was possible to use 16 bits for the
       32-bit vector corrloQQ, but it didn't work. */

    WebRtcIsacfix_AutocorrFix(corrhiQQ,DataHiQ6,WINLEN, ORDERHI, &scale);

    QdomHI = 12-scale; // QdomHI is the Q-domain of corrhiQQ
    sh_hi = WebRtcSpl_NormW32(corrhiQQ[0]);
    QdomHI += sh_hi;
    for (ii=0; ii<ORDERHI+1; ii++) {
      corrhiQQ[ii] = WEBRTC_SPL_LSHIFT_W32(corrhiQQ[ii], sh_hi);
    }

    /* less noise for lower frequencies, by filtering/scaling autocorrelation sequences */

    /* Calculate corrlo2[0] = tmpQQlo * corrlo[0] - 2.0*tmpQQlo * corrlo[1];*/
    corrlo2QQ[0] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQlo, corrloQQ[0]), 1)- // Q(12+QdomLO-16)>>1 = Q(QdomLO-5)
        WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(aaQ14, corrloQQ[1]), 2); // 2*Q(14+QdomLO-16)>>3 = Q(QdomLO-2)>>2 = Q(QdomLO-5)

    /* Calculate corrlo2[n] = tmpQQlo * corrlo[n] - tmpQQlo * (corrlo[n-1] + corrlo[n+1]);*/
    for (n = 1; n <= ORDERLO; n++) {

      tmp32 = WEBRTC_SPL_RSHIFT_W32(corrloQQ[n-1], 1) + WEBRTC_SPL_RSHIFT_W32(corrloQQ[n+1], 1); // Q(QdomLO-1)
      corrlo2QQ[n] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQlo, corrloQQ[n]), 1)- // Q(12+QdomLO-16)>>1 = Q(QdomLO-5)
          WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(aaQ14, tmp32), 2); // Q(14+QdomLO-1-16)>>2 = Q(QdomLO-3)>>2 = Q(QdomLO-5)

    }
    QdomLO -= 5;

    /* Calculate corrhi[n] = tmpQQhi * corrhi[n]; */
    for (n = 0; n <= ORDERHI; n++) {
      corrhiQQ[n] = WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQhi, corrhiQQ[n]); // Q(12+QdomHI-16) = Q(QdomHI-4)
    }
    QdomHI -= 4;

    /* add white noise floor */
    /* corrlo2QQ is in Q(QdomLO) and corrhiQQ is in Q(QdomHI) */
    /* Calculate corrlo2[0] += 9.5367431640625e-7; and
       corrhi[0]  += 9.5367431640625e-7, where the constant is 1/2^20 */

    tmp32 = WEBRTC_SPL_SHIFT_W32((WebRtc_Word32) 1, QdomLO-20);
    corrlo2QQ[0] += tmp32;
    tmp32 = WEBRTC_SPL_SHIFT_W32((WebRtc_Word32) 1, QdomHI-20);
    corrhiQQ[0]  += tmp32;

    /* corrlo2QQ is in Q(QdomLO) and corrhiQQ is in Q(QdomHI) before the following
       code segment, where we want to make sure we get a 1-bit margin */
    for (n = 0; n <= ORDERLO; n++) {
      corrlo2QQ[n] = WEBRTC_SPL_RSHIFT_W32(corrlo2QQ[n], 1); // Make sure we have a 1-bit margin
    }
    QdomLO -= 1; // Now, corrlo2QQ is in Q(QdomLO), with a 1-bit margin

    for (n = 0; n <= ORDERHI; n++) {
      corrhiQQ[n] = WEBRTC_SPL_RSHIFT_W32(corrhiQQ[n], 1); // Make sure we have a 1-bit margin
    }
    QdomHI -= 1; // Now, corrhiQQ is in Q(QdomHI), with a 1-bit margin


    newQdomLO = QdomLO;

    for (n = 0; n <= ORDERLO; n++) {
      WebRtc_Word32 tmp, tmpB, tmpCorr;
      WebRtc_Word16 alpha=328; //0.01 in Q15
      WebRtc_Word16 beta=324; //(1-0.01)*0.01=0.0099 in Q15
      WebRtc_Word16 gamma=32440; //(1-0.01)=0.99 in Q15

      if (maskdata->CorrBufLoQQ[n] != 0) {
        shMem=WebRtcSpl_NormW32(maskdata->CorrBufLoQQ[n]);
        sh = QdomLO - maskdata->CorrBufLoQdom[n];
        if (sh<=shMem) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], sh); // Get CorrBufLoQQ to same domain as corrlo2
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha, tmp);
        } else if ((sh-shMem)<7){
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], shMem); // Shift up CorrBufLoQQ as much as possible
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(WEBRTC_SPL_LSHIFT_W16(alpha, (sh-shMem)), tmp); // Shift alpha the number of times required to get tmp in QdomLO
        } else {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], shMem); // Shift up CorrBufHiQQ as much as possible
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(WEBRTC_SPL_LSHIFT_W16(alpha, 6), tmp); // Shift alpha as much as possible without overflow the number of times required to get tmp in QdomHI
          tmpCorr = WEBRTC_SPL_RSHIFT_W32(corrloQQ[n], sh-shMem-6);
          tmp = tmp + tmpCorr;
          maskdata->CorrBufLoQQ[n] = tmp;
          newQdomLO = QdomLO-(sh-shMem-6);
          maskdata->CorrBufLoQdom[n] = newQdomLO;
        }
      } else
        tmp = 0;

      tmp = tmp + corrlo2QQ[n];

      maskdata->CorrBufLoQQ[n] = tmp;
      maskdata->CorrBufLoQdom[n] = QdomLO;

      tmp=WEBRTC_SPL_MUL_16_32_RSFT15(beta, tmp);
      tmpB=WEBRTC_SPL_MUL_16_32_RSFT15(gamma, corrlo2QQ[n]);
      corrlo2QQ[n] = tmp + tmpB;
    }
    if( newQdomLO!=QdomLO) {
      for (n = 0; n <= ORDERLO; n++) {
        if (maskdata->CorrBufLoQdom[n] != newQdomLO)
          corrloQQ[n] = WEBRTC_SPL_RSHIFT_W32(corrloQQ[n], maskdata->CorrBufLoQdom[n]-newQdomLO);
      }
      QdomLO = newQdomLO;
    }


    newQdomHI = QdomHI;

    for (n = 0; n <= ORDERHI; n++) {
      WebRtc_Word32 tmp, tmpB, tmpCorr;
      WebRtc_Word16 alpha=328; //0.01 in Q15
      WebRtc_Word16 beta=324; //(1-0.01)*0.01=0.0099 in Q15
      WebRtc_Word16 gamma=32440; //(1-0.01)=0.99 in Q1
      if (maskdata->CorrBufHiQQ[n] != 0) {
        shMem=WebRtcSpl_NormW32(maskdata->CorrBufHiQQ[n]);
        sh = QdomHI - maskdata->CorrBufHiQdom[n];
        if (sh<=shMem) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], sh); // Get CorrBufHiQQ to same domain as corrhi
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha, tmp);
          tmpCorr = corrhiQQ[n];
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          maskdata->CorrBufHiQdom[n] = QdomHI;
        } else if ((sh-shMem)<7) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], shMem); // Shift up CorrBufHiQQ as much as possible
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(WEBRTC_SPL_LSHIFT_W16(alpha, (sh-shMem)), tmp); // Shift alpha the number of times required to get tmp in QdomHI
          tmpCorr = corrhiQQ[n];
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          maskdata->CorrBufHiQdom[n] = QdomHI;
        } else {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], shMem); // Shift up CorrBufHiQQ as much as possible
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(WEBRTC_SPL_LSHIFT_W16(alpha, 6), tmp); // Shift alpha as much as possible without overflow the number of times required to get tmp in QdomHI
          tmpCorr = WEBRTC_SPL_RSHIFT_W32(corrhiQQ[n], sh-shMem-6);
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          newQdomHI = QdomHI-(sh-shMem-6);
          maskdata->CorrBufHiQdom[n] = newQdomHI;
        }
      } else {
        tmp = corrhiQQ[n];
        tmpCorr = tmp;
        maskdata->CorrBufHiQQ[n] = tmp;
        maskdata->CorrBufHiQdom[n] = QdomHI;
      }

      tmp=WEBRTC_SPL_MUL_16_32_RSFT15(beta, tmp);
      tmpB=WEBRTC_SPL_MUL_16_32_RSFT15(gamma, tmpCorr);
      corrhiQQ[n] = tmp + tmpB;
    }

    if( newQdomHI!=QdomHI) {
      for (n = 0; n <= ORDERHI; n++) {
        if (maskdata->CorrBufHiQdom[n] != newQdomHI)
          corrhiQQ[n] = WEBRTC_SPL_RSHIFT_W32(corrhiQQ[n], maskdata->CorrBufHiQdom[n]-newQdomHI);
      }
      QdomHI = newQdomHI;
    }

    stab=WebRtcSpl_LevinsonW32_JSK(corrlo2QQ, a_LOQ11, k_vecloQ15, ORDERLO);

    if (stab<0) {  // If unstable use lower order
      a_LOQ11[0]=2048;
      for (n = 1; n <= ORDERLO; n++) {
        a_LOQ11[n]=0;
      }

      stab=WebRtcSpl_LevinsonW32_JSK(corrlo2QQ, a_LOQ11, k_vecloQ15, 8);
    }


    WebRtcSpl_LevinsonDurbin(corrhiQQ,  a_HIQ12,  k_vechiQ15, ORDERHI);

    /* bandwidth expansion */
    for (n = 1; n <= ORDERLO; n++) {
      a_LOQ11[n] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT_WITH_FIXROUND(kPolyVecLo[n-1], a_LOQ11[n]);
    }


    polyHI[0] = a_HIQ12[0];
    for (n = 1; n <= ORDERHI; n++) {
      a_HIQ12[n] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT_WITH_FIXROUND(kPolyVecHi[n-1], a_HIQ12[n]);
      polyHI[n] = a_HIQ12[n];
    }

    /* Normalize the corrlo2 vector */
    sh = WebRtcSpl_NormW32(corrlo2QQ[0]);
    for (n = 0; n <= ORDERLO; n++) {
      corrlo2QQ[n] = WEBRTC_SPL_LSHIFT_W32(corrlo2QQ[n], sh);
    }
    QdomLO += sh; /* Now, corrlo2QQ is still in Q(QdomLO) */


    /* residual energy */

    sh_lo = 31;
    res_nrgQQ = 0;
    for (j = 0; j <= ORDERLO; j++)
    {
      for (n = 0; n < j; n++)
      {
        WebRtc_Word16 index, diff, sh_corr;

        index = j - n; //WEBRTC_SPL_ABS_W16(j-n);

        /* Calculation of res_nrg += a_LO[j] * corrlo2[j-n] * a_LO[n]; */
        /* corrlo2QQ is in Q(QdomLO) */
        tmp32 = ((WebRtc_Word32) WEBRTC_SPL_MUL_16_16(a_LOQ11[j], a_LOQ11[n])); // Q11*Q11 = Q22
        // multiply by 2 as loop only on half of the matrix. a_LOQ11 gone through bandwidth
        // expation so the following shift is safe.
        tmp32 = WEBRTC_SPL_LSHIFT_W32(tmp32, 1);
        sh = WebRtcSpl_NormW32(tmp32);
        aSQR32 = WEBRTC_SPL_LSHIFT_W32(tmp32, sh); // Q(22+sh)
        sh_corr = WebRtcSpl_NormW32(corrlo2QQ[index]);
        tmp32 = WEBRTC_SPL_LSHIFT_W32(corrlo2QQ[index], sh_corr);
        tmp32 = (WebRtc_Word32) WEBRTC_SPL_MUL_32_32_RSFT32BI(aSQR32, tmp32); // Q(22+sh)*Q(QdomLO+sh_corr)>>32 = Q(22+sh+QdomLO+sh_corr-32) = Q(sh+QdomLO+sh_corr-10)
        sh = sh+QdomLO+sh_corr-10;

        diff = sh_lo-sh;

        round = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, (WEBRTC_SPL_ABS_W32(diff)-1));
        if (diff==0)
          round = 0;
        if (diff>=31) {
          res_nrgQQ = tmp32;
          sh_lo = sh;
        } else if (diff>0) {
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32((res_nrgQQ+round), (diff+1)) + WEBRTC_SPL_RSHIFT_W32(tmp32, 1);
          sh_lo = sh-1;
        } else  if (diff>-31){
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1) + WEBRTC_SPL_SHIFT_W32((tmp32+round), -(-diff+1));
          sh_lo = sh_lo-1;
        }
        sh = WebRtcSpl_NormW32(res_nrgQQ);
        res_nrgQQ = WEBRTC_SPL_LSHIFT_W32(res_nrgQQ, sh);
        sh_lo += sh;
      }
      n = j;
      {
        WebRtc_Word16 index, diff, sh_corr;

        index = 0; //WEBRTC_SPL_ABS_W16(j-n);

        /* Calculation of res_nrg += a_LO[j] * corrlo2[j-n] * a_LO[n]; */
        /* corrlo2QQ is in Q(QdomLO) */
        tmp32 = (WebRtc_Word32) WEBRTC_SPL_MUL_16_16(a_LOQ11[j], a_LOQ11[n]); // Q11*Q11 = Q22
        sh = WebRtcSpl_NormW32(tmp32);
        aSQR32 = WEBRTC_SPL_LSHIFT_W32(tmp32, sh); // Q(22+sh)
        sh_corr = WebRtcSpl_NormW32(corrlo2QQ[index]);
        tmp32 = WEBRTC_SPL_LSHIFT_W32(corrlo2QQ[index], sh_corr);
        tmp32 = (WebRtc_Word32) WEBRTC_SPL_MUL_32_32_RSFT32BI(aSQR32, tmp32); // Q(22+sh)*Q(QdomLO+sh_corr)>>32 = Q(22+sh+QdomLO+sh_corr-32) = Q(sh+QdomLO+sh_corr-10)
        sh = sh+QdomLO+sh_corr-10;
        diff = sh_lo-sh;

        round = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, (WEBRTC_SPL_ABS_W32(diff)-1));
        if (diff==0)
          round = 0;
        if (diff>=31) {
          res_nrgQQ = tmp32;
          sh_lo = sh;
        } else if (diff>0) {
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32((res_nrgQQ+round), (diff+1)) + WEBRTC_SPL_RSHIFT_W32(tmp32, 1);
          sh_lo = sh-1;
        } else  if (diff>-31){
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1) + WEBRTC_SPL_SHIFT_W32((tmp32+round), -(-diff+1));
          sh_lo = sh_lo-1;
        }
        sh = WebRtcSpl_NormW32(res_nrgQQ);
        res_nrgQQ = WEBRTC_SPL_LSHIFT_W32(res_nrgQQ, sh);
        sh_lo += sh;
      }
    }
    /* Convert to reflection coefficients */


    WebRtcSpl_AToK_JSK(a_LOQ11, ORDERLO, rcQ15_lo);

    if (sh_lo & 0x0001) {
      res_nrgQQ=WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1);
      sh_lo-=1;
    }


    if( res_nrgQQ > 0 )
    {
      sqrt_nrg=WebRtcSpl_Sqrt(res_nrgQQ);

      /* add hearing threshold and compute the gain */
      /* lo_coeff = varscale * S_N_R / (sqrt_nrg + varscale * H_T_H); */


      //tmp32a=WEBRTC_SPL_MUL_16_16_RSFT(varscaleQ14, H_T_HQ19, 17);  // Q14
      tmp32a=WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32) varscaleQ14,1);  // H_T_HQ19=65536 (16-17=-1)   ssh= WEBRTC_SPL_RSHIFT_W16(sh_lo, 1);  // sqrt_nrg is in Qssh
      ssh= WEBRTC_SPL_RSHIFT_W16(sh_lo, 1);  // sqrt_nrg is in Qssh
      sh = ssh - 14;
      tmp32b = WEBRTC_SPL_SHIFT_W32(tmp32a, sh); // Q14->Qssh
      tmp32c = sqrt_nrg + tmp32b;  // Qssh  (denominator)
      tmp32a = WEBRTC_SPL_MUL_16_16_RSFT(varscaleQ14, snrq, 0);  //Q24 (numerator)

      sh = WebRtcSpl_NormW32(tmp32c);
      shft = 16 - sh;
      tmp16a = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(tmp32c, -shft); // Q(ssh-shft)  (denominator)

      tmp32b = WebRtcSpl_DivW32W16(tmp32a, tmp16a); // Q(24-ssh+shft)
      sh = ssh-shft-7;
      *gain_lo_hiQ17 = WEBRTC_SPL_SHIFT_W32(tmp32b, sh);  // Gains in Q17
    }
    else
    {
      *gain_lo_hiQ17 = 100; //(WebRtc_Word32)WEBRTC_SPL_LSHIFT_W32( (WebRtc_Word32)1, 17);  // Gains in Q17
    }
    gain_lo_hiQ17++;

    /* copy coefficients to output array */
    for (n = 0; n < ORDERLO; n++) {
      *lo_coeffQ15 = (WebRtc_Word16) (rcQ15_lo[n]);
      lo_coeffQ15++;
    }
    /* residual energy */
    res_nrgQQ = 0;
    sh_hi = 31;


    for (j = 0; j <= ORDERHI; j++)
    {
      for (n = 0; n < j; n++)
      {
        WebRtc_Word16 index, diff, sh_corr;

        index = j-n; //WEBRTC_SPL_ABS_W16(j-n);

        /* Calculation of res_nrg += a_HI[j] * corrhi[j-n] * a_HI[n] * 2; for j != n */
        /* corrhiQQ is in Q(QdomHI) */
        tmp32 = ((WebRtc_Word32) WEBRTC_SPL_MUL_16_16(a_HIQ12[j], a_HIQ12[n])); // Q12*Q12 = Q24
        tmp32 = WEBRTC_SPL_LSHIFT_W32(tmp32, 1);
        sh = WebRtcSpl_NormW32(tmp32);
        aSQR32 = WEBRTC_SPL_LSHIFT_W32(tmp32, sh); // Q(24+sh)
        sh_corr = WebRtcSpl_NormW32(corrhiQQ[index]);
        tmp32 = WEBRTC_SPL_LSHIFT_W32(corrhiQQ[index],sh_corr);
        tmp32 = (WebRtc_Word32) WEBRTC_SPL_MUL_32_32_RSFT32BI(aSQR32, tmp32); // Q(24+sh)*Q(QdomHI+sh_corr)>>32 = Q(24+sh+QdomHI+sh_corr-32) = Q(sh+QdomHI+sh_corr-8)
        sh = sh+QdomHI+sh_corr-8;
        diff = sh_hi-sh;

        round = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, (WEBRTC_SPL_ABS_W32(diff)-1));
        if (diff==0)
          round = 0;
        if (diff>=31) {
          res_nrgQQ = tmp32;
          sh_hi = sh;
        } else if (diff>0) {
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32((res_nrgQQ+round), (diff+1)) + WEBRTC_SPL_RSHIFT_W32(tmp32, 1);
          sh_hi = sh-1;
        } else  if (diff>-31){
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1) + WEBRTC_SPL_SHIFT_W32((tmp32+round), -(-diff+1));
          sh_hi = sh_hi-1;
        }

        sh = WebRtcSpl_NormW32(res_nrgQQ);
        res_nrgQQ = WEBRTC_SPL_LSHIFT_W32(res_nrgQQ, sh);
        sh_hi += sh;
      }

      n = j;
      {
        WebRtc_Word16 index, diff, sh_corr;

        index = 0; //n-j; //WEBRTC_SPL_ABS_W16(j-n);

        /* Calculation of res_nrg += a_HI[j] * corrhi[j-n] * a_HI[n];*/
        /* corrhiQQ is in Q(QdomHI) */
        tmp32 = ((WebRtc_Word32) WEBRTC_SPL_MUL_16_16(a_HIQ12[j], a_HIQ12[n])); // Q12*Q12 = Q24
        sh = WebRtcSpl_NormW32(tmp32);
        aSQR32 = WEBRTC_SPL_LSHIFT_W32(tmp32, sh); // Q(24+sh)
        sh_corr = WebRtcSpl_NormW32(corrhiQQ[index]);
        tmp32 = WEBRTC_SPL_LSHIFT_W32(corrhiQQ[index],sh_corr);
        tmp32 = (WebRtc_Word32) WEBRTC_SPL_MUL_32_32_RSFT32BI(aSQR32, tmp32); // Q(24+sh)*Q(QdomHI+sh_corr)>>32 = Q(24+sh+QdomHI+sh_corr-32) = Q(sh+QdomHI+sh_corr-8)
        sh = sh+QdomHI+sh_corr-8;
        diff = sh_hi-sh;

        round = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, (WEBRTC_SPL_ABS_W32(diff)-1));
        if (diff==0)
          round = 0;
        if (diff>=31) {
          res_nrgQQ = tmp32;
          sh_hi = sh;
        } else if (diff>0) {
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32((res_nrgQQ+round), (diff+1)) + WEBRTC_SPL_RSHIFT_W32(tmp32, 1);
          sh_hi = sh-1;
        } else  if (diff>-31){
          res_nrgQQ = WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1) + WEBRTC_SPL_SHIFT_W32((tmp32+round), -(-diff+1));
          sh_hi = sh_hi-1;
        }

        sh = WebRtcSpl_NormW32(res_nrgQQ);
        res_nrgQQ = WEBRTC_SPL_LSHIFT_W32(res_nrgQQ, sh);
        sh_hi += sh;
      }
    }

    /* Convert to reflection coefficients */
    WebRtcSpl_LpcToReflCoef(polyHI, ORDERHI, rcQ15_hi);

    if (sh_hi & 0x0001) {
      res_nrgQQ=WEBRTC_SPL_RSHIFT_W32(res_nrgQQ, 1);
      sh_hi-=1;
    }


    if( res_nrgQQ > 0 )
    {
      sqrt_nrg=WebRtcSpl_Sqrt(res_nrgQQ);


      /* add hearing threshold and compute the gain */
      /* hi_coeff = varscale * S_N_R / (sqrt_nrg + varscale * H_T_H); */

      //tmp32a=WEBRTC_SPL_MUL_16_16_RSFT(varscaleQ14, H_T_HQ19, 17);  // Q14
      tmp32a=WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32) varscaleQ14,1);  // H_T_HQ19=65536 (16-17=-1)

      ssh= WEBRTC_SPL_RSHIFT_W32(sh_hi, 1);  // sqrt_nrg is in Qssh
      sh = ssh - 14;
      tmp32b = WEBRTC_SPL_SHIFT_W32(tmp32a, sh); // Q14->Qssh
      tmp32c = sqrt_nrg + tmp32b;  // Qssh  (denominator)
      tmp32a = WEBRTC_SPL_MUL_16_16_RSFT(varscaleQ14, snrq, 0);  //Q24 (numerator)

      sh = WebRtcSpl_NormW32(tmp32c);
      shft = 16 - sh;
      tmp16a = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(tmp32c, -shft); // Q(ssh-shft)  (denominator)

      tmp32b = WebRtcSpl_DivW32W16(tmp32a, tmp16a); // Q(24-ssh+shft)
      sh = ssh-shft-7;
      *gain_lo_hiQ17 = WEBRTC_SPL_SHIFT_W32(tmp32b, sh);  // Gains in Q17
    }
    else
    {
      *gain_lo_hiQ17 = 100; //(WebRtc_Word32)WEBRTC_SPL_LSHIFT_W32( (WebRtc_Word32)1, 17);  // Gains in Q17
    }
    gain_lo_hiQ17++;


    /* copy coefficients to output array */
    for (n = 0; n < ORDERHI; n++) {
      *hi_coeffQ15 = rcQ15_hi[n];
      hi_coeffQ15++;
    }
  }
}
