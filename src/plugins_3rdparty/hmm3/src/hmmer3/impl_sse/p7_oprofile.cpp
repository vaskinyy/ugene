/* Routines for the P7_OPROFILE structure:  
* a search profile in an optimized implementation.
* 
* Contents:
*   1. The P7_OPROFILE object: allocation, initialization, destruction.
*   2. Conversion from generic P7_PROFILE to optimized P7_OPROFILE
*   3. Debugging and development utilities.
*   4. Benchmark driver.
*   5. Unit tests.
*   6. Test driver.
*   7. Example.
*   8. Copyright and license information.
*   
* SRE, Wed Jul 30 11:00:04 2008 [Janelia]
 * SVN $Id: p7_oprofile.c 3018 2009-10-29 17:33:06Z farrarm $
*/
#include <hmmer3/p7_config.h>

#include <stdio.h>
#include <string.h>
#include <math.h>		/* roundf() */

#include <xmmintrin.h>		/* SSE  */
#include <emmintrin.h>		/* SSE2 */

#include <hmmer3/easel/easel.h>
#include <hmmer3/easel/esl_random.h>
#include <hmmer3/easel/esl_sse.h>
#include <hmmer3/easel/esl_vectorops.h>

#include <hmmer3/hmmer.h>
#include <hmmer3/impl_sse/impl_sse.h>


/*****************************************************************
* 1. The P7_OPROFILE structure: a score profile.
*****************************************************************/

/* Function:  p7_oprofile_Create()
* Synopsis:  Allocate an optimized profile structure.
* Incept:    SRE, Sun Nov 25 12:03:19 2007 [Casa de Gatos]
*
* Purpose:   Allocate for profiles of up to <allocM> nodes for digital alphabet <abc>.
*
* Throws:    <NULL> on allocation error.
*/
P7_OPROFILE *
p7_oprofile_Create(int allocM, const ESL_ALPHABET *abc)
{
    int          status;
    P7_OPROFILE *om  = NULL;
    int          nqb = p7O_NQB(allocM); /* # of uchar vectors needed for query */
    int          nqw = p7O_NQW(allocM); /* # of sword vectors needed for query */
    int          nqf = p7O_NQF(allocM); /* # of float vectors needed for query */
    int          x;

    /* level 0 */
    ESL_ALLOC_WITH_TYPE(om, P7_OPROFILE*, sizeof(P7_OPROFILE));
    om->rbv_mem = NULL;
    om->rwv_mem = NULL;
    om->twv_mem = NULL;
    om->rfv_mem = NULL;
    om->tfv_mem = NULL;
    om->rbv     = NULL;
    om->rwv     = NULL;
    om->twv     = NULL;
    om->rfv     = NULL;
    om->tfv     = NULL;
  om->clone   = 0;

    /* level 1 */
    ESL_ALLOC_WITH_TYPE(om->rbv_mem, __m128i*, sizeof(__m128i) * nqb  * abc->Kp          +15);	/* +15 is for manual 16-byte alignment */
    ESL_ALLOC_WITH_TYPE(om->rwv_mem, __m128i*, sizeof(__m128i) * nqw  * abc->Kp          +15);                     
    ESL_ALLOC_WITH_TYPE(om->twv_mem,__m128i*,  sizeof(__m128i) * nqw  * p7O_NTRANS       +15);   
    ESL_ALLOC_WITH_TYPE(om->rfv_mem,__m128*,   sizeof(__m128)  * nqf  * abc->Kp          +15);                     
    ESL_ALLOC_WITH_TYPE(om->tfv_mem,__m128*,   sizeof(__m128)  * nqf  * p7O_NTRANS       +15);    

    ESL_ALLOC_WITH_TYPE(om->rbv, __m128i**, sizeof(__m128i *) * abc->Kp); 
    ESL_ALLOC_WITH_TYPE(om->rwv, __m128i**, sizeof(__m128i *) * abc->Kp); 
    ESL_ALLOC_WITH_TYPE(om->rfv, __m128**,  sizeof(__m128  *) * abc->Kp); 

    /* align vector memory on 16-byte boundaries */
  om->rbv[0] = (__m128i *) (((unsigned long int) om->rbv_mem + 15) & (~0xf));
  om->rwv[0] = (__m128i *) (((unsigned long int) om->rwv_mem + 15) & (~0xf));
  om->twv    = (__m128i *) (((unsigned long int) om->twv_mem + 15) & (~0xf));
  om->rfv[0] = (__m128  *) (((unsigned long int) om->rfv_mem + 15) & (~0xf));
  om->tfv    = (__m128  *) (((unsigned long int) om->tfv_mem + 15) & (~0xf));

    /* set the rest of the row pointers for match emissions */
    for (x = 1; x < abc->Kp; x++) {
    om->rbv[x] = om->rbv[0] + (x * nqb);
    om->rwv[x] = om->rwv[0] + (x * nqw);
    om->rfv[x] = om->rfv[0] + (x * nqf);
    }
    om->allocQ16  = nqb;
    om->allocQ8   = nqw;
    om->allocQ4   = nqf;

    /* Remaining initializations */
    om->tbm_b     = 0;
    om->tec_b     = 0;
    om->tjb_b     = 0;
    om->scale_b   = 0.0f;
    om->base_b    = 0;
    om->bias_b    = 0;

    om->scale_w      = 0.0f;
    om->base_w       = 0;
    om->ddbound_w    = 0;
    om->ncj_roundoff = 0.0f;	

    for (x = 0; x < p7_NOFFSETS; x++) om->offs[x]    = -1;
    for (x = 0; x < p7_NEVPARAM; x++) om->evparam[x] = p7_EVPARAM_UNSET;
    for (x = 0; x < p7_NCUTOFFS; x++) om->cutoff[x]  = p7_CUTOFF_UNSET;
    for (x = 0; x < p7_MAXABET;  x++) om->compo[x]   = p7_COMPO_UNSET;

    om->name      = NULL;
    om->acc       = NULL;
    om->desc      = NULL;

    /* in a P7_OPROFILE, we always allocate for the optional RF, CS annotation.  
    * we only rely on the leading \0 to signal that it's unused, but 
    * we initialize all this memory to zeros to shut valgrind up about 
    * fwrite'ing uninitialized memory in the io functions.
    */
    ESL_ALLOC_WITH_TYPE(om->rf, char*,         sizeof(char) * (allocM+2)); 
    ESL_ALLOC_WITH_TYPE(om->cs, char*,         sizeof(char) * (allocM+2));
    ESL_ALLOC_WITH_TYPE(om->consensus, char*,   sizeof(char) * (allocM+2));
    memset(om->rf,      '\0', sizeof(char) * (allocM+2));
    memset(om->cs,       '\0', sizeof(char) * (allocM+2));
    memset(om->consensus,'\0', sizeof(char) * (allocM+2));

    om->abc       = abc;
    om->L         = 0;
    om->M         = 0;
    om->allocM    = allocM;
    om->mode      = p7_NO_MODE;
    om->nj        = 0.0f;
    return om;

ERROR:
    p7_oprofile_Destroy(om);
    return NULL;
}

/* Function:  p7_oprofile_IsLocal()
* Synopsis:  Returns TRUE if profile is in local alignment mode.
* Incept:    SRE, Sat Aug 16 08:46:00 2008 [Janelia]
*/
int
p7_oprofile_IsLocal(const P7_OPROFILE *om)
{
    if (om->mode == p7_LOCAL || om->mode == p7_UNILOCAL) return TRUE;
    return FALSE;
}



/* Function:  p7_oprofile_Destroy()
* Synopsis:  Frees an optimized profile structure.
* Incept:    SRE, Sun Nov 25 12:22:21 2007 [Casa de Gatos]
*/
void
p7_oprofile_Destroy(P7_OPROFILE *om)
{
    if (om == NULL) return;

  if (om->clone == 0)
    {
      if (om->rbv_mem   != NULL) free(om->rbv_mem);
      if (om->rwv_mem   != NULL) free(om->rwv_mem);
      if (om->twv_mem   != NULL) free(om->twv_mem);
      if (om->rfv_mem   != NULL) free(om->rfv_mem);
      if (om->tfv_mem   != NULL) free(om->tfv_mem);
      if (om->rbv       != NULL) free(om->rbv);
      if (om->rwv       != NULL) free(om->rwv);
      if (om->rfv       != NULL) free(om->rfv);
    if (om->name      != NULL) free(om->name);
    if (om->acc       != NULL) free(om->acc);
    if (om->desc      != NULL) free(om->desc);
      if (om->rf        != NULL) free(om->rf);
    if (om->cs        != NULL) free(om->cs);
    if (om->consensus != NULL) free(om->consensus);
    }

    free(om);
}

/* Function:  p7_oprofile_Copy()
 * Synopsis:  Allocate an optimized profile structure.
 * Incept:    SRE, Sun Nov 25 12:03:19 2007 [Casa de Gatos]
 *
 * Purpose:   Allocate for profiles of up to <allocM> nodes for digital alphabet <abc>.
 *
 * Throws:    <NULL> on allocation error.
 */
P7_OPROFILE *
p7_oprofile_Copy(P7_OPROFILE *om1)
{
  int           x, y;
  int           status;

  int           nqb  = p7O_NQB(om1->allocM); /* # of uchar vectors needed for query */
  int           nqw  = p7O_NQW(om1->allocM); /* # of sword vectors needed for query */
  int           nqf  = p7O_NQF(om1->allocM); /* # of float vectors needed for query */

  size_t        size = sizeof(char) * (om1->allocM+2);

  P7_OPROFILE  *om2  = NULL;
  
  const ESL_ALPHABET *abc = om1->abc;

  /* level 0 */
  ESL_ALLOC_WITH_TYPE(om2, P7_OPROFILE*, sizeof(P7_OPROFILE));
  om2->rbv_mem = NULL;
  om2->rwv_mem = NULL;
  om2->twv_mem = NULL;
  om2->rfv_mem = NULL;
  om2->tfv_mem = NULL;
  om2->rbv     = NULL;
  om2->rwv     = NULL;
  om2->twv     = NULL;
  om2->rfv     = NULL;
  om2->tfv     = NULL;

  /* level 1 */
  ESL_ALLOC_WITH_TYPE(om2->rbv_mem, __m128i*, sizeof(__m128i) * nqb  * abc->Kp    +15);	/* +15 is for manual 16-byte alignment */
  ESL_ALLOC_WITH_TYPE(om2->rwv_mem, __m128i*, sizeof(__m128i) * nqw  * abc->Kp    +15);                     
  ESL_ALLOC_WITH_TYPE(om2->twv_mem, __m128i*, sizeof(__m128i) * nqw  * p7O_NTRANS +15);   
  ESL_ALLOC_WITH_TYPE(om2->rfv_mem, __m128*,  sizeof(__m128)  * nqf  * abc->Kp    +15);                     
  ESL_ALLOC_WITH_TYPE(om2->tfv_mem, __m128*,  sizeof(__m128)  * nqf  * p7O_NTRANS +15);    

  ESL_ALLOC_WITH_TYPE(om2->rbv, __m128i**,  sizeof(__m128i *) * abc->Kp); 
  ESL_ALLOC_WITH_TYPE(om2->rwv, __m128i**,  sizeof(__m128i *) * abc->Kp); 
  ESL_ALLOC_WITH_TYPE(om2->rfv, __m128**,   sizeof(__m128  *) * abc->Kp); 

  /* align vector memory on 16-byte boundaries */
  om2->rbv[0] = (__m128i *) (((unsigned long int) om2->rbv_mem + 15) & (~0xf));
  om2->rwv[0] = (__m128i *) (((unsigned long int) om2->rwv_mem + 15) & (~0xf));
  om2->twv    = (__m128i *) (((unsigned long int) om2->twv_mem + 15) & (~0xf));
  om2->rfv[0] = (__m128  *) (((unsigned long int) om2->rfv_mem + 15) & (~0xf));
  om2->tfv    = (__m128  *) (((unsigned long int) om2->tfv_mem + 15) & (~0xf));

  /* copy the vector data */
  memcpy(om2->rbv[0], om1->rbv[0], sizeof(__m128i) * nqb  * abc->Kp);
  memcpy(om2->rwv[0], om1->rwv[0], sizeof(__m128i) * nqw  * abc->Kp);
  memcpy(om2->rfv[0], om1->rfv[0], sizeof(__m128i) * nqf  * abc->Kp);

  /* set the rest of the row pointers for match emissions */
  for (x = 1; x < abc->Kp; x++) {
    om2->rbv[x] = om2->rbv[0] + (x * nqb);
    om2->rwv[x] = om2->rwv[0] + (x * nqw);
    om2->rfv[x] = om2->rfv[0] + (x * nqf);
  }
  om2->allocQ16  = nqb;
  om2->allocQ8   = nqw;
  om2->allocQ4   = nqf;

  /* Remaining initializations */
  om2->tbm_b     = om1->tbm_b;
  om2->tec_b     = om1->tec_b;
  om2->tjb_b     = om1->tjb_b;
  om2->scale_b   = om1->scale_b;
  om2->base_b    = om1->base_b;
  om2->bias_b    = om1->bias_b;

  om2->scale_w      = om1->scale_w;
  om2->base_w       = om1->base_w;
  om2->ddbound_w    = om1->ddbound_w;
  om2->ncj_roundoff = om1->ncj_roundoff;	

  for (x = 0; x < p7_NOFFSETS; x++) om2->offs[x]    = om1->offs[x];
  for (x = 0; x < p7_NEVPARAM; x++) om2->evparam[x] = om1->evparam[x];
  for (x = 0; x < p7_NCUTOFFS; x++) om2->cutoff[x]  = om1->cutoff[x];
  for (x = 0; x < p7_MAXABET;  x++) om2->compo[x]   = om1->compo[x];

  for (x = 0; x < nqw  * p7O_NTRANS; ++x) om2->twv[x] = om1->twv[x];
  for (x = 0; x < nqf  * p7O_NTRANS; ++x) om2->tfv[x] = om1->tfv[x];

  for (x = 0; x < p7O_NXSTATES; x++)
    for (y = 0; y < p7O_NXTRANS; y++)
      {
	om2->xw[x][y] = om1->xw[x][y];
	om2->xf[x][y] = om1->xf[x][y];
      }

  if ((status = esl_strdup(om1->name, -1, &om2->name)) != eslOK) goto ERROR;
  if ((status = esl_strdup(om1->acc,  -1, &om2->acc))  != eslOK) goto ERROR;
  if ((status = esl_strdup(om1->desc, -1, &om2->desc)) != eslOK) goto ERROR;

  /* in a P7_OPROFILE, we always allocate for the optional RF, CS annotation.  
   * we only rely on the leading \0 to signal that it's unused, but 
   * we initialize all this memory to zeros to shut valgrind up about 
   * fwrite'ing uninitialized memory in the io functions.
   */
  ESL_ALLOC_WITH_TYPE(om2->rf, char*,          size); 
  ESL_ALLOC_WITH_TYPE(om2->cs, char*,          size);
  ESL_ALLOC_WITH_TYPE(om2->consensus, char*,  size);

  memcpy(om2->rf,        om1->rf,        size);
  memcpy(om2->cs,        om1->cs,        size);
  memcpy(om2->consensus, om1->consensus, size);

  om2->abc       = om1->abc;
  om2->L         = om1->L;
  om2->M         = om1->M;
  om2->allocM    = om1->allocM;
  om2->mode      = om1->mode;
  om2->nj        = om1->nj;

  om2->clone     = om1->clone;

  return om2;

 ERROR:
  p7_oprofile_Destroy(om2);
  return NULL;
}

/* Function:  p7_oprofile_Clone()
 * Synopsis:  Allocate a cloned copy of the optimized profile structure.  All
 *            allocated memory from the original profile is not reallocated.
 *            The cloned copy will point to the same memory as the original.
 * Incept:    SRE, Sun Nov 25 12:03:19 2007 [Casa de Gatos]
 *
 * Purpose:   Quick copy of an optimized profile used in mutiple threads.
 *
 * Throws:    <NULL> on allocation error.
 */
P7_OPROFILE *
p7_oprofile_Clone(const P7_OPROFILE *om1)
{
  int           status;

  P7_OPROFILE  *om2  = NULL;

  ESL_ALLOC_WITH_TYPE(om2, P7_OPROFILE*, sizeof(P7_OPROFILE));
  memcpy(om2, om1, sizeof(P7_OPROFILE));

  om2->clone  = 1;

  return om2;

 ERROR:
  p7_oprofile_Destroy(om2);
  return NULL;
}

/*----------------- end, P7_OPROFILE structure ------------------*/



/*****************************************************************
* 2. Conversion from generic P7_PROFILE to optimized P7_OPROFILE
*****************************************************************/

/* biased_byteify()
* Converts original log-odds residue score to a rounded biased uchar cost.
* Match emission scores for MSVFilter get this treatment.
* e.g. a score of +3.2, with scale 3.0 and bias 12, becomes 2.
*    3.2*3 = 9.6; rounded = 10; bias-10 = 2.
* When used, we add the bias, then subtract this cost.
* (A cost of +255 is our -infinity "prohibited event")
*/
static uint8_t
biased_byteify(P7_OPROFILE *om, float sc)
{
    uint8_t b;

    sc  = -1.0f * roundf(om->scale_b * sc);                          /* ugh. sc is now an integer cost represented in a float...           */
    b   = (sc > 255 - om->bias_b) ? 255 : (uint8_t) sc + om->bias_b; /* and now we cast, saturate, and bias it to an unsigned char cost... */
    return b;
}

/* unbiased_byteify()
* Convert original transition score to a rounded uchar cost
* Transition scores for MSVFilter get this treatment.
* e.g. a score of -2.1, with scale 3.0, becomes a cost of 6.
* (A cost of +255 is our -infinity "prohibited event")
*/
static uint8_t 
unbiased_byteify(P7_OPROFILE *om, float sc)
{
    uint8_t b;

    sc  = -1.0f * roundf(om->scale_b * sc);       /* ugh. sc is now an integer cost represented in a float...    */
    b   = (sc > 255.) ? 255 : (uint8_t) sc;	/* and now we cast and saturate it to an unsigned char cost... */
    return b;
}

/* wordify()
* Converts log probability score to a rounded signed 16-bit integer cost.
* Both emissions and transitions for ViterbiFilter get this treatment.
* No bias term needed, because we use signed words. 
*   e.g. a score of +3.2, with scale 500.0, becomes +1600.
*/
static int16_t 
wordify(P7_OPROFILE *om, float sc)
{
    sc  = roundf(om->scale_w * sc);
    if      (sc >=  32767.0) return  32767;
    else if (sc <= -32768.0) return -32768;
    else return (int16_t) sc;
}

/* mf_conversion(): 
* 
* This builds the MSVFilter() parts of the profile <om>, scores
* in lspace uchars (16-way parallel), by rescaling, rounding, and
* casting the scores in <gm>.
* 
* Returns <eslOK> on success;
* throws <eslEINVAL> if <om> hasn't been allocated properly.
*/
static int
mf_conversion(const P7_PROFILE *gm, P7_OPROFILE *om)
{
    int     M   = gm->M;		/* length of the query                                          */
    int     nq  = p7O_NQB(M);     /* segment length; total # of striped vectors needed            */
    float   max = 0.0;		/* maximum residue score: used for unsigned emission score bias */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* the usual counter over model nodes 1..M                      */
    int     z;			/* counter within elements of one SIMD minivector               */
    union { __m128i v; uint8_t i[16]; } tmp; /* used to align and load simd minivectors           */

    if (nq > om->allocQ16) ESL_EXCEPTION(eslEINVAL, "optimized profile is too small to hold conversion");

    /* First we determine the basis for the limited-precision MSVFilter scoring system. 
    * Default: 1/3 bit units, base offset 190:  range 0..255 => -190..65 => -63.3..21.7 bits
    * See J2/66, J4/138 for analysis.
    */
    for (x = 0; x < gm->abc->K; x++)  max = ESL_MAX(max, esl_vec_FMax(gm->rsc[x], (M+1)*2));
    om->scale_b = 3.0 / eslCONST_LOG2;                    /* scores in units of third-bits */
  om->base_b  = 190;
  om->bias_b  = unbiased_byteify(om, -1.0 * max);

    /* striped match costs: start at k=1.  */
    for (x = 0; x < gm->abc->Kp; x++)
        for (q = 0, k = 1; q < nq; q++, k++)
        {
            for (z = 0; z < 16; z++) tmp.i[z] = ((k+ z*nq <= M) ? biased_byteify(om, p7P_MSC(gm, k+z*nq, x)) : 255);
	om->rbv[x][q]   = tmp.v;	
        }

        /* transition costs */
        om->tbm_b = unbiased_byteify(om, logf(2.0f / ((float) gm->M * (float) (gm->M+1)))); /* constant B->Mk penalty        */
        om->tec_b = unbiased_byteify(om, logf(0.5f));                                       /* constant multihit E->C = E->J */
        om->tjb_b = unbiased_byteify(om, logf(3.0f / (float) (gm->L+3))); /* this adopts the L setting of the parent profile */

        return eslOK;
}


/* vf_conversion(): 
* 
* This builds the ViterbiFilter() parts of the profile <om>, scores
* in lspace swords (8-way parallel), by rescaling, rounding, and
* casting the scores in <gm>.
* 
* Returns <eslOK> on success;
* throws <eslEINVAL> if <om> hasn't been allocated properly.
*/
static int
vf_conversion(const P7_PROFILE *gm, P7_OPROFILE *om)
{
    int     M   = gm->M;		/* length of the query                                          */
    int     nq  = p7O_NQW(M);     /* segment length; total # of striped vectors needed            */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* the usual counter over model nodes 1..M                      */
    int     kb;			/* possibly offset base k for loading om's TSC vectors          */
    int     z;			/* counter within elements of one SIMD minivector               */
    int     t;			/* counter over transitions 0..7 = p7O_{BM,MM,IM,DM,MD,MI,II,DD}*/
    int     tg;			/* transition index in gm                                       */
    int     j;			/* counter in interleaved vector arrays in the profile          */
    int     ddtmp;		/* used in finding worst DD transition bound                    */
    int16_t  maxval;		/* used to prevent zero cost II                                 */
    int16_t  val;
    union { __m128i v; int16_t i[8]; } tmp; /* used to align and load simd minivectors            */

    if (nq > om->allocQ8) ESL_EXCEPTION(eslEINVAL, "optimized profile is too small to hold conversion");

    /* First set the basis for the limited-precision scoring system. 
    * Default: 1/500 bit units, base offset 12000:  range -32768..32767 => -44768..20767 => -89.54..41.53 bits
    * See J4/138 for analysis.
    */
    om->scale_w = 500.0 / eslCONST_LOG2;
    om->base_w  = 12000;

    /* striped match scores */
    for (x = 0; x < gm->abc->Kp; x++)
        for (k = 1, q = 0; q < nq; q++, k++)
        {
            for (z = 0; z < 8; z++) tmp.i[z] = ((k+ z*nq <= M) ? wordify(om, p7P_MSC(gm, k+z*nq, x)) : -32768);
	om->rwv[x][q]   = tmp.v;
        }

        /* Transition costs, all but the DD's. */
        for (j = 0, k = 1, q = 0; q < nq; q++, k++)
        {
            for (t = p7O_BM; t <= p7O_II; t++) /* this loop of 7 transitions depends on the order in p7o_tsc_e */
            {
                switch (t) {
      case p7O_BM: tg = p7P_BM;  kb = k-1; maxval =  0; break; /* gm has tBMk stored off by one! start from k=0 not 1   */
      case p7O_MM: tg = p7P_MM;  kb = k-1; maxval =  0; break; /* MM, DM, IM vectors are rotated by -1, start from k=0  */
      case p7O_IM: tg = p7P_IM;  kb = k-1; maxval =  0; break;
      case p7O_DM: tg = p7P_DM;  kb = k-1; maxval =  0; break;
      case p7O_MD: tg = p7P_MD;  kb = k;   maxval =  0; break; /* the remaining ones are straight up  */
      case p7O_MI: tg = p7P_MI;  kb = k;   maxval =  0; break; 
      case p7O_II: tg = p7P_II;  kb = k;   maxval = -1; break; 
                }

                for (z = 0; z < 8; z++) {
                    val      = ((kb+ z*nq < M) ? wordify(om, p7P_TSC(gm, kb+ z*nq, tg)) : -32768);
                    tmp.i[z] = (val <= maxval) ? val : maxval; /* do not allow an II transition cost of 0, or hell may occur. */
                }
	  om->twv[j++] = tmp.v;
            }
        }

        /* Finally the DD's, which are at the end of the optimized tsc vector; (j is already sitting there) */
        for (k = 1, q = 0; q < nq; q++, k++)
        {
            for (z = 0; z < 8; z++) tmp.i[z] = ((k+ z*nq < M) ? wordify(om, p7P_TSC(gm, k+ z*nq, p7P_DD)) : -32768);
      om->twv[j++] = tmp.v;
        }

        /* Specials. (Actually in same order in om and gm, but we copy in general form anyway.)  */
        /* VF CC,NN,JJ transitions hardcoded zero; -3.0 nat approximation used instead; this papers
        * over a length independence problem, where the approximation weirdly outperforms the
        * exact solution, probably indicating that the model's Pascal distribution is problematic,
        * and the "approximation" is in fact closer to the One True Model, the mythic H4 supermodel.
        * [xref J5/36] 
        */
        om->xw[p7O_E][p7O_LOOP] = wordify(om, gm->xsc[p7P_E][p7P_LOOP]);  
        om->xw[p7O_E][p7O_MOVE] = wordify(om, gm->xsc[p7P_E][p7P_MOVE]);
        om->xw[p7O_N][p7O_MOVE] = wordify(om, gm->xsc[p7P_N][p7P_MOVE]);
        om->xw[p7O_N][p7O_LOOP] = 0;                                        /* was wordify(om, gm->xsc[p7P_N][p7P_LOOP]); */
        om->xw[p7O_C][p7O_MOVE] = wordify(om, gm->xsc[p7P_C][p7P_MOVE]);
        om->xw[p7O_C][p7O_LOOP] = 0;                                        /* was wordify(om, gm->xsc[p7P_C][p7P_LOOP]); */
        om->xw[p7O_J][p7O_MOVE] = wordify(om, gm->xsc[p7P_J][p7P_MOVE]);
        om->xw[p7O_J][p7O_LOOP] = 0;                                        /* was wordify(om, gm->xsc[p7P_J][p7P_LOOP]); */

        om->ncj_roundoff = 0.0; /* goes along with NN=CC=JJ=0, -3.0 nat approximation */
        /* otherwise, would be = om->scale_w * gm->xsc[p7P_N][p7P_LOOP] -  om->xw[p7O_N][p7O_LOOP];   */
        /* see J4/150 for discussion of VF error suppression, superceded by the -3.0 nat approximation */

        /* Transition score bound for "lazy F" DD path evaluation (xref J2/52) */
        om->ddbound_w = -32768;	
        for (k = 2; k < M-1; k++) 
        {
            ddtmp         = (int) wordify(om, p7P_TSC(gm, k,   p7P_DD));
            ddtmp        += (int) wordify(om, p7P_TSC(gm, k+1, p7P_DM));
            ddtmp        -= (int) wordify(om, p7P_TSC(gm, k+1, p7P_BM));
            om->ddbound_w = ESL_MAX(om->ddbound_w, ddtmp);
        }

        return eslOK;
}


/* fb_conversion()
* This builds the Forward/Backward part of the optimized profile <om>,
* where we use odds ratios (not log-odds scores).
*/
static int
fb_conversion(const P7_PROFILE *gm, P7_OPROFILE *om)
{
    int     M   = gm->M;		/* length of the query                                          */
    int     nq  = p7O_NQF(M);     /* segment length; total # of striped vectors needed            */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* the usual counter over model nodes 1..M                      */
    int     kb;			/* possibly offset base k for loading om's TSC vectors          */
    int     z;			/* counter within elements of one SIMD minivector               */
    int     t;			/* counter over transitions 0..7 = p7O_{BM,MM,IM,DM,MD,MI,II,DD}*/
    int     tg;			/* transition index in gm                                       */
    int     j;			/* counter in interleaved vector arrays in the profile          */
    union { __m128 v; float x[4]; } tmp; /* used to align and load simd minivectors               */

    if (nq > om->allocQ4) ESL_EXCEPTION(eslEINVAL, "optimized profile is too small to hold conversion");

    /* striped match scores: start at k=1 */
    for (x = 0; x < gm->abc->Kp; x++)
        for (k = 1, q = 0; q < nq; q++, k++)
        {
            for (z = 0; z < 4; z++) tmp.x[z] = (k+ z*nq <= M) ? p7P_MSC(gm, k+z*nq, x) : -eslINFINITY;
	om->rfv[x][q] = esl_sse_expf(tmp.v);
        }

        /* Transition scores, all but the DD's. */
        for (j = 0, k = 1, q = 0; q < nq; q++, k++)
        {
            for (t = p7O_BM; t <= p7O_II; t++) /* this loop of 7 transitions depends on the order in the definition of p7o_tsc_e */
            {
                switch (t) {
      case p7O_BM: tg = p7P_BM;  kb = k-1; break; /* gm has tBMk stored off by one! start from k=0 not 1 */
      case p7O_MM: tg = p7P_MM;  kb = k-1; break; /* MM, DM, IM quads are rotated by -1, start from k=0  */
      case p7O_IM: tg = p7P_IM;  kb = k-1; break;
      case p7O_DM: tg = p7P_DM;  kb = k-1; break;
      case p7O_MD: tg = p7P_MD;  kb = k;   break; /* the remaining ones are straight up  */
      case p7O_MI: tg = p7P_MI;  kb = k;   break; 
      case p7O_II: tg = p7P_II;  kb = k;   break; 
                }

                for (z = 0; z < 4; z++) tmp.x[z] = (kb+z*nq < M) ? p7P_TSC(gm, kb+z*nq, tg) : -eslINFINITY;
	  om->tfv[j++] = esl_sse_expf(tmp.v);
            }
        }

        /* And finally the DD's, which are at the end of the optimized tsc vector; (j is already there) */
        for (k = 1, q = 0; q < nq; q++, k++)
        {
            for (z = 0; z < 4; z++) tmp.x[z] = (k+z*nq < M) ? p7P_TSC(gm, k+z*nq, p7P_DD) : -eslINFINITY;
      om->tfv[j++] = esl_sse_expf(tmp.v);
        }

        /* Specials. (These are actually in exactly the same order in om and
        *  gm, but we copy in general form anyway.)
        */
        om->xf[p7O_E][p7O_LOOP] = expf(gm->xsc[p7P_E][p7P_LOOP]);  
        om->xf[p7O_E][p7O_MOVE] = expf(gm->xsc[p7P_E][p7P_MOVE]);
        om->xf[p7O_N][p7O_LOOP] = expf(gm->xsc[p7P_N][p7P_LOOP]);
        om->xf[p7O_N][p7O_MOVE] = expf(gm->xsc[p7P_N][p7P_MOVE]);
        om->xf[p7O_C][p7O_LOOP] = expf(gm->xsc[p7P_C][p7P_LOOP]);
        om->xf[p7O_C][p7O_MOVE] = expf(gm->xsc[p7P_C][p7P_MOVE]);
        om->xf[p7O_J][p7O_LOOP] = expf(gm->xsc[p7P_J][p7P_LOOP]);
        om->xf[p7O_J][p7O_MOVE] = expf(gm->xsc[p7P_J][p7P_MOVE]);

        return eslOK;
}


/* Function:  p7_oprofile_Convert()
* Synopsis:  Converts standard profile to an optimized one.
* Incept:    SRE, Mon Nov 26 07:38:57 2007 [Janelia]
*
* Purpose:   Convert a standard profile <gm> to an optimized profile <om>,
*            where <om> has already been allocated for a profile of at 
*            least <gm->M> nodes and the same emission alphabet <gm->abc>.
*
* Args:      gm - profile to optimize
*            om - allocated optimized profile for holding the result.
*
* Returns:   <eslOK> on success.
*
* Throws:    <eslEINVAL> if <gm>, <om> aren't compatible. 
*            <eslEMEM> on allocation failure.
*/
int
p7_oprofile_Convert(const P7_PROFILE *gm, P7_OPROFILE *om)
{
    int status, z;

    if (gm->abc->type != om->abc->type)  ESL_EXCEPTION(eslEINVAL, "alphabets of the two profiles don't match");  
    if (gm->M         >  om->allocM)     ESL_EXCEPTION(eslEINVAL, "oprofile is too small");  

    if ((status =  mf_conversion(gm, om)) != eslOK) return status;   /* MSVFilter()'s information     */
    if ((status =  vf_conversion(gm, om)) != eslOK) return status;   /* ViterbiFilter()'s information */
    if ((status =  fb_conversion(gm, om)) != eslOK) return status;   /* ForwardFilter()'s information */

    if (om->name != NULL) free(om->name);
    if (om->acc  != NULL) free(om->acc);
    if (om->desc != NULL) free(om->desc);
    if ((status = esl_strdup(gm->name, -1, &(om->name))) != eslOK) goto ERROR;
    if ((status = esl_strdup(gm->acc,  -1, &(om->acc)))  != eslOK) goto ERROR;
    if ((status = esl_strdup(gm->desc, -1, &(om->desc))) != eslOK) goto ERROR;
  strcpy(om->rf,        gm->rf);
    strcpy(om->cs,        gm->cs);
    strcpy(om->consensus, gm->consensus);
    for (z = 0; z < p7_NEVPARAM; z++) om->evparam[z] = gm->evparam[z];
    for (z = 0; z < p7_NCUTOFFS; z++) om->cutoff[z]  = gm->cutoff[z];
    for (z = 0; z < p7_MAXABET;  z++) om->compo[z]   = gm->compo[z];

    om->mode = gm->mode;
    om->L    = gm->L;
    om->M    = gm->M;
    om->nj   = gm->nj;
    return eslOK;

ERROR:
    return status;
}

/* Function:  p7_oprofile_ReconfigLength()
* Synopsis:  Set the target sequence length of a model.
* Incept:    SRE, Thu Dec 20 09:56:40 2007 [Janelia]
*
* Purpose:   Given an already configured model <om>, quickly reset its
*            expected length distribution for a new mean target sequence
*            length of <L>. 
*            
*            This doesn't affect the length distribution of the null
*            model. That must also be reset, using <p7_bg_SetLength()>.
*            
*            We want this routine to run as fast as possible, because
*            this call is in the critical path: it must be called at
*            each new target sequence in a database search.
*
* Returns:   <eslOK> on success. Costs/scores for N,C,J transitions are set
*            here.
*/
int
p7_oprofile_ReconfigLength(P7_OPROFILE *om, int L, int wholeSeqSz)
{
    int status;
    if ((status = p7_oprofile_ReconfigMSVLength (om, wholeSeqSz)) != eslOK) return status;
    if ((status = p7_oprofile_ReconfigRestLength(om, L, wholeSeqSz)) != eslOK) return status;
    return eslOK;
}

/* Function:  p7_oprofile_ReconfigMSVLength()
* Synopsis:  Set the target sequence length of the MSVFilter part of the model.
* Incept:    SRE, Tue Dec 16 13:39:17 2008 [Janelia]
*
* Purpose:   Given an  already configured model <om>, quickly reset its
*            expected length distribution for a new mean target sequence
*            length of <L>, only for the part of the model that's used
*            for the accelerated MSV filter.
*            
*            The acceleration pipeline uses this to defer reconfiguring the
*            length distribution of the main model, mostly because hmmscan
*            reads the model in two pieces, MSV part first, then the rest.
*
* Returns:   <eslOK> on success.
*/
int
p7_oprofile_ReconfigMSVLength(P7_OPROFILE *om, int L)
{
    om->tjb_b = unbiased_byteify(om, logf(3.0f / (float) (L+3)));
    return eslOK;
}

/* Function:  p7_oprofile_ReconfigRestLength()
* Synopsis:  Set the target sequence length of the main profile.
* Incept:    SRE, Tue Dec 16 13:41:30 2008 [Janelia]
*
* Purpose:   Given an  already configured model <om>, quickly reset its
*            expected length distribution for a new mean target sequence
*            length of <L>, for everything except the MSV filter part
*            of the model.
*            
*            Calling <p7_oprofile_ReconfigMSVLength()> then
*            <p7_oprofile_ReconfigRestLength()> is equivalent to
*            just calling <p7_oprofile_ReconfigLength()>. The two
*            part version is used in the acceleration pipeline.
*
* Returns:   <eslOK> on success.           
*/
int
p7_oprofile_ReconfigRestLength(P7_OPROFILE *om, int L, int wholeSz)
{
    float pmove, ploop;

    pmove = (2.0f + om->nj) / ((float) wholeSz + 2.0f + om->nj); /* 2/(L+2) for sw; 3/(L+3) for fs */
    ploop = 1.0f - pmove;

    /* ForwardFilter() parameters: pspace floats */
    om->xf[p7O_N][p7O_LOOP] =  om->xf[p7O_C][p7O_LOOP] = om->xf[p7O_J][p7O_LOOP] = ploop;
    om->xf[p7O_N][p7O_MOVE] =  om->xf[p7O_C][p7O_MOVE] = om->xf[p7O_J][p7O_MOVE] = pmove;

    /* ViterbiFilter() parameters: lspace signed 16-bit ints */
    om->xw[p7O_N][p7O_MOVE] =  om->xw[p7O_C][p7O_MOVE] = om->xw[p7O_J][p7O_MOVE] = wordify(om, logf(pmove));
    /* om->xw[p7O_N][p7O_LOOP] =  om->xw[p7O_C][p7O_LOOP] = om->xw[p7O_J][p7O_LOOP] = wordify(om, logf(ploop)); */ /* 3nat approx in force: these stay 0 */
    /* om->ncj_roundoff        = (om->scale_w * logf(ploop)) - om->xw[p7O_N][p7O_LOOP];                         */ /* and this does too                  */

    om->L = L;
    return eslOK;
}


/* Function:  p7_oprofile_ReconfigMultihit()
* Synopsis:  Quickly reconfig model into multihit mode for target length <L>.
* Incept:    SRE, Thu Aug 21 10:04:07 2008 [Janelia]
*
* Purpose:   Given a profile <om> that's already been configured once,
*            quickly reconfigure it into a multihit mode for target 
*            length <L>. 
*            
*            This gets called in domain definition, when we need to
*            flip the model in and out of unihit mode to
*            process individual domains.
*            
* Note:      You can't just flip uni/multi mode alone, because that
*            parameterization also affects target length
*            modeling. You need to make sure uni vs. multi choice is
*            made before the length model is set, and you need to
*            make sure the length model is recalculated if you change
*            the uni/multi mode. Hence, these functions call
*            <p7_oprofile_ReconfigLength()>.
*/
int
p7_oprofile_ReconfigMultihit(P7_OPROFILE *om, int L, int wholeSz)
{
    om->xf[p7O_E][p7O_MOVE] = 0.5;
    om->xf[p7O_E][p7O_LOOP] = 0.5;
    om->nj = 1.0f;

    om->xw[p7O_E][p7O_MOVE] = wordify(om, -eslCONST_LOG2);
    om->xw[p7O_E][p7O_LOOP] = wordify(om, -eslCONST_LOG2);

    return p7_oprofile_ReconfigLength(om, L, wholeSz);
}

/* Function:  p7_oprofile_ReconfigUnihit()
* Synopsis:  Quickly reconfig model into unihit mode for target length <L>.
* Incept:    SRE, Thu Aug 21 10:10:32 2008 [Janelia]
*
* Purpose:   Given a profile <om> that's already been configured once,
*            quickly reconfigure it into a unihit mode for target 
*            length <L>. 
*            
*            This gets called in domain definition, when we need to
*            flip the model in and out of unihit <L=0> mode to
*            process individual domains.
*/
int
p7_oprofile_ReconfigUnihit(P7_OPROFILE *om, int L, int wholeSz)
{
    om->xf[p7O_E][p7O_MOVE] = 1.0f;
    om->xf[p7O_E][p7O_LOOP] = 0.0f;
    om->nj = 0.0f;

    om->xw[p7O_E][p7O_MOVE] = 0;
    om->xw[p7O_E][p7O_LOOP] = -32768;

    return p7_oprofile_ReconfigLength(om, L, wholeSz);
}
/*------------ end, conversions to P7_OPROFILE ------------------*/



/*****************************************************************
* 3. Debugging and development utilities.
*****************************************************************/


/* oprofile_dump_mf()
* 
* Dump the MSVFilter part of a profile <om> to <stdout>.
*/
static int
oprofile_dump_mf(FILE *fp, const P7_OPROFILE *om)
{
    int     M   = om->M;		/* length of the query                                          */
    int     nq  = p7O_NQB(M);     /* segment length; total # of striped vectors needed            */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* counter over nodes 1..M                                      */
    int     z;			/* counter within elements of one SIMD minivector               */
    union { __m128i v; uint8_t i[16]; } tmp; /* used to align and read simd minivectors           */

    /* Header (rearranged column numbers, in the vectors)  */
    fprintf(fp, "     ");
    for (k =1, q = 0; q < nq; q++, k++)
    {
        fprintf(fp, "[ ");
        for (z = 0; z < 16; z++) 
            if (k+z*nq <= M) fprintf(fp, "%4d ", k+z*nq);
            else             fprintf(fp, "%4s ", "xx");
            fprintf(fp, "]");
    }
    fprintf(fp, "\n");

    /* Table of residue emissions */
    for (x = 0; x < om->abc->Kp; x++)
    {
        fprintf(fp, "(%c): ", om->abc->sym[x]); 

        for (q = 0; q < nq; q++)
        {
            fprintf(fp, "[ ");
	  _mm_store_si128(&tmp.v, om->rbv[x][q]);
            for (z = 0; z < 16; z++) fprintf(fp, "%4d ", tmp.i[z]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");

    fprintf(fp, "t_EC,EJ:    %4d\n",  om->tec_b);
    fprintf(fp, "t_NB,JB,CT: %4d\n",  om->tjb_b);
    fprintf(fp, "t_BMk:      %4d\n",  om->tbm_b);
    fprintf(fp, "scale:      %.2f\n", om->scale_b);
    fprintf(fp, "base:       %4d\n",  om->base_b);
    fprintf(fp, "bias:       %4d\n",  om->bias_b);
    fprintf(fp, "Q:          %4d\n",  nq);  
    fprintf(fp, "M:          %4d\n",  M);  
    return eslOK;
}



/* oprofile_dump_vf()
* 
* Dump the ViterbiFilter part of a profile <om> to <stdout>.
*/
static int
oprofile_dump_vf(FILE *fp, const P7_OPROFILE *om)
{
    int     M   = om->M;		/* length of the query                                          */
    int     nq  = p7O_NQW(M);     /* segment length; total # of striped vectors needed            */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* the usual counter over model nodes 1..M                      */
    int     kb;			/* possibly offset base k for loading om's TSC vectors          */
    int     z;			/* counter within elements of one SIMD minivector               */
    int     t;			/* counter over transitions 0..7 = p7O_{BM,MM,IM,DM,MD,MI,II,DD}*/
    int     j;			/* counter in interleaved vector arrays in the profile          */
    union { __m128i v; int16_t i[8]; } tmp; /* used to align and read simd minivectors           */

    /* Emission score header (rearranged column numbers, in the vectors)  */
    fprintf(fp, "     ");
    for (k =1, q = 0; q < nq; q++, k++)
    {
        fprintf(fp, "[ ");
        for (z = 0; z < 8; z++) 
            if (k+z*nq <= M) fprintf(fp, "%6d ", k+z*nq);
            else             fprintf(fp, "%6s ", "xx");
            fprintf(fp, "]");
    }
    fprintf(fp, "\n");

    /* Table of residue emissions */
    for (x = 0; x < om->abc->Kp; x++)
    {
        fprintf(fp, "(%c): ", om->abc->sym[x]); 

        /* Match emission scores (insert emissions are assumed zero by design) */
        for (q = 0; q < nq; q++)
        {
            fprintf(fp, "[ ");
	  _mm_store_si128(&tmp.v, om->rwv[x][q]);
            for (z = 0; z < 8; z++) fprintf(fp, "%6d ", tmp.i[z]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");

    /* Transitions */
    for (t = p7O_BM; t <= p7O_II; t++)
    {
        switch (t) {
      case p7O_BM: fprintf(fp, "\ntBM: "); break;
      case p7O_MM: fprintf(fp, "\ntMM: "); break;
      case p7O_IM: fprintf(fp, "\ntIM: "); break;
      case p7O_DM: fprintf(fp, "\ntDM: "); break;
      case p7O_MD: fprintf(fp, "\ntMD: "); break;
      case p7O_MI: fprintf(fp, "\ntMI: "); break;
      case p7O_II: fprintf(fp, "\ntII: "); break;
        }

        for (k = 1, q = 0; q < nq; q++, k++)
        {
            switch (t) {
        case p7O_BM: kb = k;                 break; 
        case p7O_MM: kb = (1 + (nq+k-2)) % nq; break; /* MM, DM, IM quads rotated by +1  */
        case p7O_IM: kb = (1 + (nq+k-2)) % nq; break;  
        case p7O_DM: kb = (1 + (nq+k-2)) % nq; break;  
        case p7O_MD: kb = k;                 break; /* the remaining ones are straight up  */
        case p7O_MI: kb = k;                 break; 
        case p7O_II: kb = k;                 break; 
            }
            fprintf(fp, "[ ");
            for (z = 0; z < 8; z++) 
                if (kb+z*nq <= M) fprintf(fp, "%6d ", kb+z*nq);
                else              fprintf(fp, "%6s ", "xx");
                fprintf(fp, "]");
        }
        fprintf(fp, "\n     ");	  
        for (q = 0; q < nq; q++)
        {
            fprintf(fp, "[ ");
	  _mm_store_si128(&tmp.v, om->twv[q*7 + t]);
            for (z = 0; z < 8; z++) fprintf(fp, "%6d ", tmp.i[z]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");	  
    }

    /* DD transitions */
    fprintf(fp, "\ntDD: ");
    for (k =1, q = 0; q < nq; q++, k++)
    {
        fprintf(fp, "[ ");
        for (z = 0; z < 8; z++) 
            if (k+z*nq <= M) fprintf(fp, "%6d ", k+z*nq);
            else             fprintf(fp, "%6s ", "xx");
            fprintf(fp, "]");
    }
    fprintf(fp, "\n     ");	  
    for (j = nq*7, q = 0; q < nq; q++, j++)
    {
        fprintf(fp, "[ ");
      _mm_store_si128(&tmp.v, om->twv[j]);
        for (z = 0; z < 8; z++) fprintf(fp, "%6d ", tmp.i[z]);
        fprintf(fp, "]");
    }
    fprintf(fp, "\n");	  

    fprintf(fp, "E->C: %6d    E->J: %6d\n", om->xw[p7O_E][p7O_MOVE], om->xw[p7O_E][p7O_LOOP]);
    fprintf(fp, "N->B: %6d    N->N: %6d\n", om->xw[p7O_N][p7O_MOVE], om->xw[p7O_N][p7O_LOOP]);
    fprintf(fp, "J->B: %6d    J->J: %6d\n", om->xw[p7O_J][p7O_MOVE], om->xw[p7O_J][p7O_LOOP]);
    fprintf(fp, "C->T: %6d    C->C: %6d\n", om->xw[p7O_C][p7O_MOVE], om->xw[p7O_C][p7O_LOOP]);

    fprintf(fp, "scale: %6.2f\n", om->scale_w);
    fprintf(fp, "base:  %6d\n",   om->base_w);
    fprintf(fp, "bound: %6d\n",   om->ddbound_w);
    fprintf(fp, "Q:     %6d\n",   nq);  
    fprintf(fp, "M:     %6d\n",   M);  
    return eslOK;
}


/* oprofile_dump_fb()
* 
* Dump the Forward/Backward part of a profile <om> to <stdout>.
* <width>, <precision> control the floating point output:
*  8,5 is a reasonable choice for prob space,
*  5,2 is reasonable for log space.
*/
static int
oprofile_dump_fb(FILE *fp, const P7_OPROFILE *om, int width, int precision)
{
    int     M   = om->M;		/* length of the query                                          */
    int     nq  = p7O_NQF(M);     /* segment length; total # of striped vectors needed            */
    int     x;			/* counter over residues                                        */
    int     q;			/* q counts over total # of striped vectors, 0..nq-1            */
    int     k;			/* the usual counter over model nodes 1..M                      */
    int     kb;			/* possibly offset base k for loading om's TSC vectors          */
    int     z;			/* counter within elements of one SIMD minivector               */
    int     t;			/* counter over transitions 0..7 = p7O_{BM,MM,IM,DM,MD,MI,II,DD}*/
    int     j;			/* counter in interleaved vector arrays in the profile          */
    union { __m128 v; float x[4]; } tmp; /* used to align and read simd minivectors               */

    /* Residue emissions */
    for (x = 0; x < om->abc->Kp; x++)
    {
        fprintf(fp, "(%c): ", om->abc->sym[x]); 
        for (k =1, q = 0; q < nq; q++, k++)
        {
            fprintf(fp, "[ ");
            for (z = 0; z < 4; z++) 
                if (k+z*nq <= M) fprintf(fp, "%*d ", width, k+z*nq);
                else             fprintf(fp, "%*s ", width, "xx");
                fprintf(fp, "]");
        }
        fprintf(fp, "\nmat: ");
        for (q = 0; q < nq; q++)
        {
            fprintf(fp, "[ ");
            tmp.v = om->rfv[x][q];
            for (z = 0; z < 4; z++) fprintf(fp, "%*.*f ", width, precision, tmp.x[z]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n\n");
    }

    /* Transitions */
    for (t = p7O_BM; t <= p7O_II; t++)
    {
        switch (t) {
      case p7O_BM: fprintf(fp, "\ntBM: "); break;
      case p7O_MM: fprintf(fp, "\ntMM: "); break;
      case p7O_IM: fprintf(fp, "\ntIM: "); break;
      case p7O_DM: fprintf(fp, "\ntDM: "); break;
      case p7O_MD: fprintf(fp, "\ntMD: "); break;
      case p7O_MI: fprintf(fp, "\ntMI: "); break;
      case p7O_II: fprintf(fp, "\ntII: "); break;
        }
        for (k = 1, q = 0; q < nq; q++, k++)
        {
            switch (t) {
        case p7O_MM:/* MM, DM, IM quads rotated by +1  */
        case p7O_IM:
        case p7O_DM:
            kb = (1 + (nq+k-2)) % nq;
            break;
        case p7O_BM:/* the remaining ones are straight up  */
        case p7O_MD:
        case p7O_MI:
        case p7O_II:
            kb = k;
            break;
            }
            fprintf(fp, "[ ");
            for (z = 0; z < 4; z++) 
                if (kb+z*nq <= M) fprintf(fp, "%*d ", width, kb+z*nq);
                else              fprintf(fp, "%*s ", width, "xx");
                fprintf(fp, "]");
        }
        fprintf(fp, "\n     ");	  
        for (q = 0; q < nq; q++)
        {
            fprintf(fp, "[ ");
            tmp.v = om->tfv[q*7 + t];
            for (z = 0; z < 4; z++) fprintf(fp, "%*.*f ", width, precision, tmp.x[z]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");	  
    }

    /* DD transitions */
    fprintf(fp, "\ntDD: ");
    for (k =1, q = 0; q < nq; q++, k++)
    {
        fprintf(fp, "[ ");
        for (z = 0; z < 4; z++) 
            if (k+z*nq <= M) fprintf(fp, "%*d ", width, k+z*nq);
            else             fprintf(fp, "%*s ", width, "xx");
            fprintf(fp, "]");
    }
    fprintf(fp, "\n     ");	  
    for (j = nq*7, q = 0; q < nq; q++, j++)
    {
        fprintf(fp, "[ ");
        tmp.v = om->tfv[j];
        for (z = 0; z < 4; z++) fprintf(fp, "%*.*f ", width, precision, tmp.x[z]);
        fprintf(fp, "]");
    }
    fprintf(fp, "\n");	  

    /* Specials */
    fprintf(fp, "E->C: %*.*f    E->J: %*.*f\n", width, precision, om->xf[p7O_E][p7O_MOVE], width, precision, om->xf[p7O_E][p7O_LOOP]);
    fprintf(fp, "N->B: %*.*f    N->N: %*.*f\n", width, precision, om->xf[p7O_N][p7O_MOVE], width, precision, om->xf[p7O_N][p7O_LOOP]);
    fprintf(fp, "J->B: %*.*f    J->J: %*.*f\n", width, precision, om->xf[p7O_J][p7O_MOVE], width, precision, om->xf[p7O_J][p7O_LOOP]);
    fprintf(fp, "C->T: %*.*f    C->C: %*.*f\n", width, precision, om->xf[p7O_C][p7O_MOVE], width, precision, om->xf[p7O_C][p7O_LOOP]);
    fprintf(fp, "Q:     %d\n",   nq);  
    fprintf(fp, "M:     %d\n",   M);  
    return eslOK;
}


/* Function:  p7_oprofile_Dump()
* Synopsis:  Dump internals of a <P7_OPROFILE>
* Incept:    SRE, Thu Dec 13 08:49:30 2007 [Janelia]
*
* Purpose:   Dump the internals of <P7_OPROFILE> structure <om>
*            to stream <fp>; generally for testing or debugging
*            purposes.
*
* Args:      fp   - output stream (often stdout)
*            om   - optimized profile to dump
*
* Returns:   <eslOK> on success.
*
* Throws:    (no abnormal error conditions)
*/
int
p7_oprofile_Dump(FILE *fp, const P7_OPROFILE *om)
{
    int status;

    fprintf(fp, "Dump of a <P7_OPROFILE> ::\n");

    fprintf(fp, "\n  -- float part, odds ratios for Forward/Backward:\n");
    if ((status = oprofile_dump_fb(fp, om, 8, 5)) != eslOK) return status;

    fprintf(fp, "\n  -- sword part, log odds for ViterbiFilter(): \n");
    if ((status = oprofile_dump_vf(fp, om))       != eslOK) return status;

    fprintf(fp, "\n  -- uchar part, log odds for MSVFilter(): \n");
    if ((status = oprofile_dump_mf(fp, om))       != eslOK) return status;

    return eslOK;
}


/* Function:  p7_oprofile_Sample()
* Synopsis:  Sample a random profile.
* Incept:    SRE, Wed Jul 30 13:11:52 2008 [Janelia]
*
* Purpose:   Sample a random profile of <M> nodes for alphabet <abc>,
*            using <r> as the source of random numbers. Parameterize
*            it for generation of target sequences of mean length
*            <L>. Calculate its log-odds scores using background
*            model <bg>.
*            
* Args:      r       - random number generator
*            abc     - emission alphabet 
*            bg      - background frequency model
*            M       - size of sampled profile, in nodes
*            L       - configured target seq mean length
*            opt_hmm - optRETURN: sampled HMM
*            opt_gm  - optRETURN: sampled normal profile
*            opt_om  - RETURN: optimized profile
*
* Returns:   <eslOK> on success.
*
* Throws:    (no abnormal error conditions)
*/
int
p7_oprofile_Sample(ESL_RANDOMNESS *r, const ESL_ALPHABET *abc, const P7_BG *bg, int M, int L,
                   P7_HMM **opt_hmm, P7_PROFILE **opt_gm, P7_OPROFILE **ret_om)
{
    P7_HMM         *hmm  = NULL;
    P7_PROFILE     *gm   = NULL;
    P7_OPROFILE    *om   = NULL;
    int             status;

    if ((gm = p7_profile_Create (M, abc)) == NULL)  { status = eslEMEM; goto ERROR; }
    if ((om = p7_oprofile_Create(M, abc)) == NULL)  { status = eslEMEM; goto ERROR; }

    if ((status = p7_hmm_Sample(r, M, abc, &hmm))             != eslOK) goto ERROR;
    if ((status = p7_ProfileConfig(hmm, bg, gm, L, p7_LOCAL)) != eslOK) goto ERROR;
    if ((status = p7_oprofile_Convert(gm, om))                != eslOK) goto ERROR;
    if ((status = p7_oprofile_ReconfigLength(om, L, L))          != eslOK) goto ERROR;

    if (opt_hmm != NULL) *opt_hmm = hmm; else p7_hmm_Destroy(hmm);
    if (opt_gm  != NULL) *opt_gm  = gm;  else p7_profile_Destroy(gm);
    *ret_om = om;
    return eslOK;

ERROR:
    if (opt_hmm != NULL) *opt_hmm = NULL;
    if (opt_gm  != NULL) *opt_gm  = NULL;
    *ret_om = NULL;
    return status;
}


/* Function:  p7_oprofile_Compare()
* Synopsis:  Compare two optimized profiles for equality.
* Incept:    SRE, Wed Jan 21 13:29:10 2009 [Janelia]
*
* Purpose:   Compare the contents of <om1> and <om2>; return 
*            <eslOK> if they are effectively identical profiles,
*            or <eslFAIL> if not.
* 
*            Floating point comparisons are done to a tolerance
*            of <tol> using <esl_FCompare()>.
*            
*            If a comparison fails, an informative error message is
*            left in <errmsg> to indicate why.
*            
*            Internal allocation sizes are not compared, only the
*            data.
*            
* Args:      om1    - one optimized profile to compare
*            om2    - the other
*            tol    - floating point comparison tolerance; see <esl_FCompare()>
*            errmsg - ptr to array of at least <eslERRBUFSIZE> characters.
*            
* Returns:   <eslOK> on effective equality;  <eslFAIL> on difference.
*/
int
p7_oprofile_Compare(const P7_OPROFILE *om1, const P7_OPROFILE *om2, float tol, char *errmsg)
{
    int Q4  = p7O_NQF(om1->M);
    int Q8  = p7O_NQW(om1->M);
    int Q16 = p7O_NQB(om1->M);
    int q, r, x, y;
    union { __m128i v; uint8_t c[16]; } a16, b16;
  union { __m128i v; int16_t w[8];  } a8,  b8;
    union { __m128  v; float   x[4];  } a4,  b4;

    if (om1->mode      != om2->mode)      ESL_FAIL(eslFAIL, errmsg, "comparison failed: mode");
    if (om1->L         != om2->L)         ESL_FAIL(eslFAIL, errmsg, "comparison failed: L");
    if (om1->M         != om2->M)         ESL_FAIL(eslFAIL, errmsg, "comparison failed: M");
    if (om1->nj        != om2->nj)        ESL_FAIL(eslFAIL, errmsg, "comparison failed: nj");
    if (om1->abc->type != om2->abc->type) ESL_FAIL(eslFAIL, errmsg, "comparison failed: alphabet type");

    /* MSVFilter part */
    for (x = 0; x < om1->abc->Kp; x++)
        for (q = 0; q < Q16; q++)
        {
	a16.v = om1->rbv[x][q]; b16.v = om2->rbv[x][q];
            for (r = 0; r < 16; r++) if (a16.c[r] != b16.c[r]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: rb[%d] elem %d", q, r);
        }
        if (om1->tbm_b     != om2->tbm_b)     ESL_FAIL(eslFAIL, errmsg, "comparison failed: tbm_b");
        if (om1->tec_b     != om2->tec_b)     ESL_FAIL(eslFAIL, errmsg, "comparison failed: tec_b");
        if (om1->tjb_b     != om2->tjb_b)     ESL_FAIL(eslFAIL, errmsg, "comparison failed: tjb_b");
        if (om1->scale_b   != om2->scale_b)   ESL_FAIL(eslFAIL, errmsg, "comparison failed: scale_b");
        if (om1->base_b    != om2->base_b)    ESL_FAIL(eslFAIL, errmsg, "comparison failed: base_b");
        if (om1->bias_b    != om2->bias_b)    ESL_FAIL(eslFAIL, errmsg, "comparison failed: bias_b");

        /* ViterbiFilter() part */
        for (x = 0; x < om1->abc->Kp; x++)
            for (q = 0; q < Q8; q++)
            {
	a8.v = om1->rwv[x][q]; b8.v = om2->rwv[x][q];
                for (r = 0; r < 8; r++) if (a8.w[r] != b8.w[r]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: rw[%d] elem %d", q, r);
            }
            for (q = 0; q < 8*Q16; q++)
            {
      a8.v = om1->twv[q]; b8.v = om2->twv[q];
                for (r = 0; r < 8; r++) if (a8.w[r] != b8.w[r]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: tw[%d] elem %d", q, r);
            }
            for (x = 0; x < p7O_NXSTATES; x++)
                for (y = 0; y < p7O_NXTRANS; y++)
                    if (om1->xw[x][y] != om2->xw[x][y]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: xw[%d][%d]", x, y);

            if (om1->scale_w   != om2->scale_w)   ESL_FAIL(eslFAIL, errmsg, "comparison failed: scale");
            if (om1->base_w    != om2->base_w)    ESL_FAIL(eslFAIL, errmsg, "comparison failed: base");
            if (om1->ddbound_w != om2->ddbound_w) ESL_FAIL(eslFAIL, errmsg, "comparison failed: ddbound_w");

            /* Forward/Backward part */
            for (x = 0; x < om1->abc->Kp; x++)
                for (q = 0; q < Q4; q++)
                {
	a4.v = om1->rfv[x][q]; b4.v = om2->rfv[x][q];
                    for (r = 0; r < 4; r++) if (esl_FCompare(a4.x[r], b4.x[r], tol) != eslOK)  ESL_FAIL(eslFAIL, errmsg, "comparison failed: rf[%d] elem %d", q, r);
                }
                for (q = 0; q < 8*Q4; q++)
                {
      a4.v = om1->tfv[q]; b4.v = om2->tfv[q];
                    for (r = 0; r < 4; r++) if (a4.x[r] != b4.x[r]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: tf[%d] elem %d", q, r);
                }
                for (x = 0; x < p7O_NXSTATES; x++)
                    if (esl_vec_FCompare(om1->xf[x], om2->xf[x], p7O_NXTRANS, tol) != eslOK) ESL_FAIL(eslFAIL, errmsg, "comparison failed: xf[%d] vector", x);

                for (x = 0; x < p7_NOFFSETS; x++)
                    if (om1->offs[x] != om2->offs[x]) ESL_FAIL(eslFAIL, errmsg, "comparison failed: offs[%d]", x);

                if (esl_strcmp(om1->name,      om2->name)      != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: name");
                if (esl_strcmp(om1->acc,       om2->acc)       != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: acc");
                if (esl_strcmp(om1->desc,      om2->desc)      != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: desc");
   if (esl_strcmp(om1->rf,        om2->rf)        != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: ref");
                if (esl_strcmp(om1->cs,        om2->cs)        != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: cs");
                if (esl_strcmp(om1->consensus, om2->consensus) != 0) ESL_FAIL(eslFAIL, errmsg, "comparison failed: consensus");

                if (esl_vec_FCompare(om1->evparam, om2->evparam, p7_NEVPARAM, tol) != eslOK) ESL_FAIL(eslFAIL, errmsg, "comparison failed: evparam vector");
                if (esl_vec_FCompare(om1->cutoff,  om2->cutoff,  p7_NCUTOFFS, tol) != eslOK) ESL_FAIL(eslFAIL, errmsg, "comparison failed: cutoff vector");
                if (esl_vec_FCompare(om1->compo,   om2->compo,   p7_MAXABET,  tol) != eslOK) ESL_FAIL(eslFAIL, errmsg, "comparison failed: compo vector");

                return eslOK;
}


/* Function:  p7_profile_SameAsMF()
* Synopsis:  Set a generic profile's scores to give MSV scores.
* Incept:    SRE, Wed Jul 30 13:42:49 2008 [Janelia]
*
* Purpose:   Set a generic profile's scores so that the normal <dp_generic> DP 
*            algorithms will give the same score as <p7_MSVFilter()>:
*            all t_MM scores = 0; all other core transitions = -inf;
*            multihit local mode; all <t_BMk> entries uniformly <log 2/(M(M+1))>;
*            <tCC, tNN, tJJ> scores 0; total approximated later as -3;
*            rounded in the same way as the 8-bit limited precision.
*
* Returns:   <eslOK> on success.
*/
int
p7_profile_SameAsMF(const P7_OPROFILE *om, P7_PROFILE *gm)
{
    int    k,x;
    float  tbm = roundf(om->scale_b * (log((double)(2.0f / ((float) gm->M * (float) (gm->M+1))))));

    /* Transitions */
    esl_vec_FSet(gm->tsc, p7P_NTRANS * gm->M, -eslINFINITY);
    for (k = 1; k <  gm->M; k++) p7P_TSC(gm, k, p7P_MM) = 0.0f;
    for (k = 0; k <  gm->M; k++) p7P_TSC(gm, k, p7P_BM) = tbm;

    /* Emissions */
    for (x = 0; x < gm->abc->Kp; x++)
        for (k = 0; k <= gm->M; k++)
        {
            gm->rsc[x][k*2]   = (gm->rsc[x][k*2] <= -eslINFINITY) ? -eslINFINITY : roundf(om->scale_b * gm->rsc[x][k*2]);
            gm->rsc[x][k*2+1] = 0;	/* insert score: VF makes it zero no matter what. */
        }	

        /* Specials */
        for (k = 0; k < p7P_NXSTATES; k++)
            for (x = 0; x < p7P_NXTRANS; x++)
                gm->xsc[k][x] = (gm->xsc[k][x] <= -eslINFINITY) ? -eslINFINITY : roundf(om->scale_b * gm->xsc[k][x]);

        /* NN, CC, JJ hardcoded 0 in limited precision */
        gm->xsc[p7P_N][p7P_LOOP] =  gm->xsc[p7P_J][p7P_LOOP] =  gm->xsc[p7P_C][p7P_LOOP] = 0;

        return eslOK;
}


/* Function:  p7_profile_SameAsVF()
* Synopsis:  Round a generic profile to match ViterbiFilter scores.
* Incept:    SRE, Wed Jul 30 13:37:48 2008 [Janelia]
*
* Purpose:   Round all the scores in a generic (lspace) <P7_PROFILE> <gm> in
*            exactly the same way that the scores in the
*            <P7_OPROFILE> <om> were rounded. Then we can test that two profiles
*            give identical internal scores in testing, say,
*            <p7_ViterbiFilter()> against <p7_GViterbi()>. 
*            
*            The 3nat approximation is used; NN=CC=JJ=0, and 3 nats are
*            subtracted at the end to account for their contribution.
*            
*            To convert a generic Viterbi score <gsc> calculated with this profile
*            to a nat score that should match ViterbiFilter() exactly,
*            do <(gsc / om->scale_w) - 3.0>.
*
*            <gm> must be the same profile that <om> was constructed from.
* 
*            <gm> is irrevocably altered by this call. 
*            
*            Do not call this more than once on any given <gm>! 
*
* Args:      <om>  - optimized profile, containing scale information.
*            <gm>  - generic profile that <om> was built from.          
*
* Returns:   <eslOK> on success.
*
* Throws:    (no abnormal error conditions)
*/
int
p7_profile_SameAsVF(const P7_OPROFILE *om, P7_PROFILE *gm)
{
    int k;
    int x;

    /* Transitions */
    /* <= -eslINFINITY test is used solely to silence compiler. really testing == -eslINFINITY */
    for (x = 0; x < gm->M*p7P_NTRANS; x++)
        gm->tsc[x] = (gm->tsc[x] <= -eslINFINITY) ? -eslINFINITY : roundf(om->scale_w * gm->tsc[x]);

    /* Enforce the rule that no II can be 0; max of -1 */
    for (x = p7P_II; x < gm->M*p7P_NTRANS; x += p7P_NTRANS) 
        if (gm->tsc[x] == 0.0) gm->tsc[x] = -1.0;

    /* Emissions */
    for (x = 0; x < gm->abc->Kp; x++)
        for (k = 0; k <= gm->M; k++)
        {
            gm->rsc[x][k*2]   = (gm->rsc[x][k*2]   <= -eslINFINITY) ? -eslINFINITY : roundf(om->scale_w * gm->rsc[x][k*2]);
            gm->rsc[x][k*2+1] = 0.0;	/* insert score: VF makes it zero no matter what. */
        }	

        /* Specials */
        for (k = 0; k < p7P_NXSTATES; k++)
            for (x = 0; x < p7P_NXTRANS; x++)
                gm->xsc[k][x] = (gm->xsc[k][x] <= -eslINFINITY) ? -eslINFINITY : roundf(om->scale_w * gm->xsc[k][x]);

        /* 3nat approximation: NN, CC, JJ hardcoded 0 in limited precision */
        gm->xsc[p7P_N][p7P_LOOP] =  gm->xsc[p7P_J][p7P_LOOP] =  gm->xsc[p7P_C][p7P_LOOP] = 0.0;

        return eslOK;
}
/*------------ end, P7_OPROFILE debugging tools  ----------------*/





/************************************************************
* HMMER - Biological sequence analysis with profile HMMs
* Version 3.0; March 2010
* Copyright (C) 2010 Howard Hughes Medical Institute.
* Other copyrights also apply. See the COPYRIGHT file for a full list.
* 
* HMMER is distributed under the terms of the GNU General Public License
* (GPLv3). See the LICENSE file for details.
************************************************************/
