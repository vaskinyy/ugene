/* SSE implementation of Forward and Backward algorithms.
*
* Both profile and DP matrix are striped and interleaved, for fast
* SIMD vector operations. Calculations are in probability space
* (scaled odds ratios, actually) rather than log probabilities,
* allowing fast multiply/add operations rather than slow Logsum()
* calculations. Sparse rescaling is used to achieve full dynamic
* range of scores.
* 
* The Forward and Backward implementations may be used either in a
* full O(ML) mode that keeps an entire DP matrix, or in a O(M+L)
* linear memory "parsing" mode that only keeps one row of memory for
* the main MDI states and one column 0..L for the special states
* B,E,N,C,J. Keeping a full matrix allows subsequent stochastic
* traceback or posterior decoding of any state. In parsing mode, we
* can do posterior decoding on the special states and determine
* regions of the target sequence that are generated by the
* nonhomology states (NCJ) versus not -- thus, identifying where
* high-probability "regions" are, the first step of identifying the
* domain structure of a target sequence.
* 
* Contents:
*   1. Forward/Backward wrapper API
*   2. Forward and Backward engine implementations
*   8. Copyright and license information.
* 
* SRE, Thu Jul 31 08:43:20 2008 [Janelia]
 * SVN $Id: fwdback.c 3018 2009-10-29 17:33:06Z farrarm $
*/

#include <hmmer3/p7_config.h>

#include <stdio.h>
#include <math.h>

#include <xmmintrin.h>		/* SSE  */
#include <emmintrin.h>		/* SSE2 */

#include <hmmer3/easel/easel.h>
#include <hmmer3/easel/esl_sse.h>

#include <hmmer3/hmmer.h>

#include <hmmer3/hmmer3_funcs.h>

#include "impl_sse.h"

// ! added percentBorder for function and taskStateInfo - to control cancel and progress
static int forward_engine (int do_full, const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, P7_OMX *fwd, float *opt_sc,
                           int percentBorder, U2::TaskStateInfo & ti );
static int backward_engine(int do_full, const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, const P7_OMX *fwd, P7_OMX *bck, float *opt_sc, 
                           int percentBorder, U2::TaskStateInfo & ti );


/*****************************************************************
* 1. Forward/Backward API.
*****************************************************************/

/* Function:  p7_Forward()
* Synopsis:  The Forward algorithm, full matrix fill version.
* Incept:    SRE, Fri Aug 15 18:59:43 2008 [Casa de Gatos]
*
* Purpose:   Calculates the Forward algorithm for sequence <dsq> of
*            length <L> residues, using optimized profile <om>, and a
*            preallocated DP matrix <ox>. Upon successful return, <ox>
*            contains the filled Forward matrix, and <*opt_sc>
*            optionally contains the raw Forward score in nats.
*            
*            This calculation requires $O(ML)$ memory and time.
*            The caller must provide a suitably allocated full <ox>
*            by calling <ox = p7_omx_Create(M, L, L)> or
*            <p7_omx_GrowTo(ox, M, L, L)>.
*
*            The model <om> must be configured in local alignment
*            mode. The sparse rescaling method used to keep
*            probability values within single-precision floating
*            point dynamic range cannot be safely applied to models in
*            glocal or global mode.
*
* Args:      dsq     - digital target sequence, 1..L
*            L       - length of dsq in residues          
*            om      - optimized profile
*            ox      - RETURN: Forward DP matrix
*            opt_sc  - RETURN: Forward score (in nats)          
*
* Returns:   <eslOK> on success. 
*
* Throws:    <eslEINVAL> if <ox> allocation is too small, or if the profile
*            isn't in local alignment mode.
*            <eslERANGE> if the score exceeds the limited range of
*            a probability-space odds ratio.
*            In either case, <*opt_sc> is undefined.
*/
int
p7_Forward(const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, P7_OMX *ox, float *opt_sc, int percentBorder, U2::TaskStateInfo & ti)
{
#ifdef p7_DEBUGGING		
    if (om->M >  ox->allocQ4*4)    ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few columns)");
    if (L     >= ox->validR)       ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few MDI rows)");
    if (L     >= ox->allocXR)      ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few X rows)");
    if (! p7_oprofile_IsLocal(om)) ESL_EXCEPTION(eslEINVAL, "Forward implementation makes assumptions that only work for local alignment");
#endif

    return forward_engine(TRUE, dsq, L, om, ox, opt_sc, percentBorder, ti );
}

/* Function:  p7_ForwardParser()
* Synopsis:  The Forward algorithm, linear memory parsing version.
* Incept:    SRE, Fri Aug 15 19:05:26 2008 [Casa de Gatos]
*
* Purpose:   Same as <p7_Forward() except that the full matrix isn't
*            kept. Instead, a linear $O(M+L)$ memory algorithm is
*            used, keeping only the DP matrix values for the special
*            (BENCJ) states. These are sufficient to do posterior
*            decoding to identify high-probability regions where
*            domains are.
* 
*            The caller must provide a suitably allocated "parsing"
*            <ox> by calling <ox = p7_omx_Create(M, 0, L)> or
*            <p7_omx_GrowTo(ox, M, 0, L)>.
*            
* Args:      dsq     - digital target sequence, 1..L
*            L       - length of dsq in residues          
*            om      - optimized profile
*            ox      - RETURN: Forward DP matrix
*            ret_sc  - RETURN: Forward score (in nats)          
*
* Returns:   <eslOK> on success.
*
* Throws:    <eslEINVAL> if <ox> allocation is too small, or if the profile
*            isn't in local alignment mode.
*            <eslERANGE> if the score exceeds the limited range of
*            a probability-space odds ratio.
*            In either case, <*opt_sc> is undefined.
*/
int
p7_ForwardParser(const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, P7_OMX *ox, float *opt_sc, int percentBorder, U2::TaskStateInfo & ti )
{
#ifdef p7_DEBUGGING		
    if (om->M >  ox->allocQ4*4)    ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few columns)");
    if (ox->validR < 1)            ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few MDI rows)");
    if (L     >= ox->allocXR)      ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few X rows)");
    if (! p7_oprofile_IsLocal(om)) ESL_EXCEPTION(eslEINVAL, "Forward implementation makes assumptions that only work for local alignment");
#endif

    return forward_engine(FALSE, dsq, L, om, ox, opt_sc, percentBorder, ti );
}



/* Function:  p7_Backward()
* Synopsis:  The Backward algorithm; full matrix fill version.
* Incept:    SRE, Sat Aug 16 08:34:22 2008 [Janelia]
*
* Purpose:   Calculates the Backward algorithm for sequence <dsq> of
*            length <L> residues, using optimized profile <om>, and a
*            preallocated DP matrix <bck>. A filled Forward matrix
*            must also be provided in <fwd>, because we need to use
*            the same sparse scaling factors that Forward used. The
*            <bck> matrix is filled in, and the Backward score (in
*            nats) is optionally returned in <opt_sc>.
*            
*            This calculation requires $O(ML)$ memory and time. The
*            caller must provide a suitably allocated full <bck> by
*            calling <bck = p7_omx_Create(M, L, L)> or
*            <p7_omx_GrowTo(bck, M, L, L)>.
*            
*            Because only the sparse scaling factors are needed from
*            the <fwd> matrix, the caller may have this matrix
*            calculated either in full or parsing mode.
*            
*            The model <om> must be configured in local alignment
*            mode. The sparse rescaling method used to keep
*            probability values within single-precision floating
*            point dynamic range cannot be safely applied to models in
*            glocal or global mode.
*
* Args:      dsq     - digital target sequence, 1..L
*            L       - length of dsq in residues          
*            om      - optimized profile
*            fwd     - filled Forward DP matrix, for scale factors
*            do_full - TRUE=full matrix; FALSE=linear memory parse mode
*            bck     - RETURN: filled Backward matrix
*            opt_sc  - optRETURN: Backward score (in nats)          
*
* Returns:   <eslOK> on success. 
*
* Throws:    <eslEINVAL> if <ox> allocation is too small, or if the profile
*            isn't in local alignment mode.
*            <eslERANGE> if the score exceeds the limited range of
*            a probability-space odds ratio.
*            In either case, <*opt_sc> is undefined.
*/
int 
p7_Backward(const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, const P7_OMX *fwd, P7_OMX *bck, float *opt_sc, int percentBorder, U2::TaskStateInfo & ti)
{
#ifdef p7_DEBUGGING		
    if (om->M >  bck->allocQ4*4)    ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few columns)");
    if (L     >= bck->validR)       ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few MDI rows)");
    if (L     >= bck->allocXR)      ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few X rows)");
    if (L     != fwd->L)            ESL_EXCEPTION(eslEINVAL, "fwd matrix size doesn't agree with length L");
    if (! p7_oprofile_IsLocal(om))  ESL_EXCEPTION(eslEINVAL, "Forward implementation makes assumptions that only work for local alignment");
#endif

    return backward_engine(TRUE, dsq, L, om, fwd, bck, opt_sc, percentBorder, ti );
}



/* Function:  p7_BackwardParser()
* Synopsis:  The Backward algorithm, linear memory parsing version.
* Incept:    SRE, Sat Aug 16 08:34:13 2008 [Janelia]
*
* Purpose:   Same as <p7_Backward()> except that the full matrix isn't
*            kept. Instead, a linear $O(M+L)$ memory algorithm is
*            used, keeping only the DP matrix values for the special
*            (BENCJ) states. These are sufficient to do posterior
*            decoding to identify high-probability regions where
*            domains are.
*       
*            The caller must provide a suitably allocated "parsing"
*            <bck> by calling <bck = p7_omx_Create(M, 0, L)> or
*            <p7_omx_GrowTo(bck, M, 0, L)>.
*
* Args:      dsq     - digital target sequence, 1..L
*            L       - length of dsq in residues          
*            om      - optimized profile
*            fwd     - filled Forward DP matrix, for scale factors
*            bck     - RETURN: filled Backward matrix
*            opt_sc  - optRETURN: Backward score (in nats)          
*
* Returns:   <eslOK> on success. 
*
* Throws:    <eslEINVAL> if <ox> allocation is too small, or if the profile
*            isn't in local alignment mode.
*            <eslERANGE> if the score exceeds the limited range of
*            a probability-space odds ratio.
*            In either case, <*opt_sc> is undefined.
*/
int 
p7_BackwardParser(const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, const P7_OMX *fwd, P7_OMX *bck, float *opt_sc, int percentBorder, U2::TaskStateInfo & ti)
{
#ifdef p7_DEBUGGING		
    if (om->M >  bck->allocQ4*4)    ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few columns)");
    if (bck->validR < 1)            ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few MDI rows)");
    if (L     >= bck->allocXR)      ESL_EXCEPTION(eslEINVAL, "DP matrix allocated too small (too few X rows)");
    if (L     != fwd->L)            ESL_EXCEPTION(eslEINVAL, "fwd matrix size doesn't agree with length L");
    if (! p7_oprofile_IsLocal(om))  ESL_EXCEPTION(eslEINVAL, "Forward implementation makes assumptions that only work for local alignment");
#endif

    return backward_engine(FALSE, dsq, L, om, fwd, bck, opt_sc, percentBorder, ti );
}



/*****************************************************************
* 2. Forward/Backward engine implementations (called thru API)
*****************************************************************/

static int
forward_engine(int do_full, const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, P7_OMX *ox, float *opt_sc, int percentBorder, U2::TaskStateInfo & ti )
{
    register __m128 mpv, dpv, ipv;   /* previous row values                                       */
    register __m128 sv;		   /* temp storage of 1 curr row value in progress              */
    register __m128 dcv;		   /* delayed storage of D(i,q+1)                               */
    register __m128 xEv;		   /* E state: keeps max for Mk->E as we go                     */
    register __m128 xBv;		   /* B state: splatted vector of B[i-1] for B->Mk calculations */
    __m128   zerov;		   /* splatted 0.0's in a vector                                */
    float    xN, xE, xB, xC, xJ;	   /* special states' scores                                    */
    int i;			   /* counter over sequence positions 1..L                      */
    int q;			   /* counter over quads 0..nq-1                                */
    int j;			   /* counter over DD iterations (4 is full serialization)      */
    int Q       = p7O_NQF(om->M);	   /* segment length: # of vectors                              */
    __m128 *dpc = ox->dpf[0];        /* current row, for use in {MDI}MO(dpp,q) access macro       */
    __m128 *dpp;                     /* previous row, for use in {MDI}MO(dpp,q) access macro      */
  __m128 *rp;			   /* will point at om->rfv[x] for residue x[i]                 */
  __m128 *tp;			   /* will point into (and step thru) om->tfv                   */

    /* Initialization. */
    ox->M  = om->M;
    ox->L  = L;
    ox->has_own_scales = TRUE; 	/* all forward matrices control their own scalefactors */
    zerov  = _mm_setzero_ps();
    for (q = 0; q < Q; q++)
        MMO(dpc,q) = IMO(dpc,q) = DMO(dpc,q) = zerov;
    xE    = ox->xmx[p7X_E] = 0.;
    xN    = ox->xmx[p7X_N] = 1.;
    xJ    = ox->xmx[p7X_J] = 0.;
    xB    = ox->xmx[p7X_B] = om->xf[p7O_N][p7O_MOVE];
    xC    = ox->xmx[p7X_C] = 0.;

    ox->xmx[p7X_SCALE] = 1.0;
    ox->totscale          = 0.0;

#if p7_DEBUGGING
    if (ox->debugging) p7_omx_DumpFBRow(ox, TRUE, 0, 9, 5, xE, xN, xJ, xB, xC);	/* logify=TRUE, <rowi>=0, width=8, precision=5*/
#endif

    // ! ADDED CODE !
	int progressStart = ti.progress;
    for (i = 1; i <= L; i++)
    {
        // ! ADDED CODE !
		ti.progress = progressStart + (int)(((double)percentBorder / L) * i);
        if( ti.cancelFlag ){ return eslCANCELED; }

        dpp   = dpc;                      
        dpc   = ox->dpf[do_full * i];     /* avoid conditional, use do_full as kronecker delta */
      rp    = om->rfv[dsq[i]];
      tp    = om->tfv;
        dcv   = _mm_setzero_ps();
        xEv   = _mm_setzero_ps();
        xBv   = _mm_set1_ps(xB);

        /* Right shifts by 4 bytes. 4,8,12,x becomes x,4,8,12.  Shift zeros on. */
        mpv   = esl_sse_rightshift_ps(MMO(dpp,Q-1), zerov);
        dpv   = esl_sse_rightshift_ps(DMO(dpp,Q-1), zerov);
        ipv   = esl_sse_rightshift_ps(IMO(dpp,Q-1), zerov);

        for (q = 0; q < Q; q++)
        {
            /* Calculate new MMO(i,q); don't store it yet, hold it in sv. */
            sv   =                _mm_mul_ps(xBv, *tp);  tp++;
            sv   = _mm_add_ps(sv, _mm_mul_ps(mpv, *tp)); tp++;
            sv   = _mm_add_ps(sv, _mm_mul_ps(ipv, *tp)); tp++;
            sv   = _mm_add_ps(sv, _mm_mul_ps(dpv, *tp)); tp++;
            sv   = _mm_mul_ps(sv, *rp);                  rp++;
            xEv  = _mm_add_ps(xEv, sv);

            /* Load {MDI}(i-1,q) into mpv, dpv, ipv;
            * {MDI}MX(q) is then the current, not the prev row
            */
            mpv = MMO(dpp,q);
            dpv = DMO(dpp,q);
            ipv = IMO(dpp,q);

            /* Do the delayed stores of {MD}(i,q) now that memory is usable */
            MMO(dpc,q) = sv;
            DMO(dpc,q) = dcv;

            /* Calculate the next D(i,q+1) partially: M->D only;
            * delay storage, holding it in dcv
            */
            dcv   = _mm_mul_ps(sv, *tp); tp++;

            /* Calculate and store I(i,q); assumes odds ratio for emission is 1.0 */
            sv         =                _mm_mul_ps(mpv, *tp);  tp++;
            IMO(dpc,q) = _mm_add_ps(sv, _mm_mul_ps(ipv, *tp)); tp++;
        }	  

        /* Now the DD paths. We would rather not serialize them but 
        * in an accurate Forward calculation, we have few options.
        */
        /* dcv has carried through from end of q loop above; store it 
        * in first pass, we add M->D and D->D path into DMX
        */
        /* We're almost certainly're obligated to do at least one complete 
        * DD path to be sure: 
        */
        dcv        = esl_sse_rightshift_ps(dcv, zerov);
        DMO(dpc,0) = zerov;
      tp         = om->tfv + 7*Q;	/* set tp to start of the DD's */
        for (q = 0; q < Q; q++) 
        {
            DMO(dpc,q) = _mm_add_ps(dcv, DMO(dpc,q));	
            dcv        = _mm_mul_ps(DMO(dpc,q), *tp); tp++; /* extend DMO(q), so we include M->D and D->D paths */
        }

        /* now. on small models, it seems best (empirically) to just go
        * ahead and serialize. on large models, we can do a bit better,
        * by testing for when dcv (DD path) accrued to DMO(q) is below
        * machine epsilon for all q, in which case we know DMO(q) are all
        * at their final values. The tradeoff point is (empirically) somewhere around M=100,
        * at least on my desktop. We don't worry about the conditional here;
        * it's outside any inner loops.
        */
        if (om->M < 100)
        {			/* Fully serialized version */
            for (j = 1; j < 4; j++)
            {
                dcv = esl_sse_rightshift_ps(dcv, zerov);
	      tp  = om->tfv + 7*Q;	/* set tp to start of the DD's */
                for (q = 0; q < Q; q++) 
                { /* note, extend dcv, not DMO(q); only adding DD paths now */
                    DMO(dpc,q) = _mm_add_ps(dcv, DMO(dpc,q));	
                    dcv        = _mm_mul_ps(dcv, *tp);   tp++; 
                }	    
            }
        } 
        else
        {			/* Slightly parallelized version, but which incurs some overhead */
            for (j = 1; j < 4; j++)
            {
                register __m128 cv;	/* keeps track of whether any DD's change DMO(q) */

                dcv = esl_sse_rightshift_ps(dcv, zerov);
	      tp  = om->tfv + 7*Q;	/* set tp to start of the DD's */
                cv  = zerov;
                for (q = 0; q < Q; q++) 
                { /* using cmpgt below tests if DD changed any DMO(q) *without* conditional branch */
                    sv         = _mm_add_ps(dcv, DMO(dpc,q));	
                    cv         = _mm_or_ps(cv, _mm_cmpgt_ps(sv, DMO(dpc,q))); 
                    DMO(dpc,q) = sv;	                                    /* store new DMO(q) */
                    dcv        = _mm_mul_ps(dcv, *tp);   tp++;            /* note, extend dcv, not DMO(q) */
                }	    
                if (! _mm_movemask_ps(cv)) break; /* DD's didn't change any DMO(q)? Then done, break out. */
            }
        }

        /* Add D's to xEv */
        for (q = 0; q < Q; q++) xEv = _mm_add_ps(DMO(dpc,q), xEv);

        /* Finally the "special" states, which start from Mk->E (->C, ->J->B) */
        /* The following incantation is a horizontal sum of xEv's elements  */
        /* These must follow DD calculations, because D's contribute to E in Forward
        * (as opposed to Viterbi)
        */
        xEv = _mm_add_ps(xEv, _mm_shuffle_ps(xEv, xEv, _MM_SHUFFLE(0, 3, 2, 1)));
        xEv = _mm_add_ps(xEv, _mm_shuffle_ps(xEv, xEv, _MM_SHUFFLE(1, 0, 3, 2)));
        _mm_store_ss(&xE, xEv);

        xN =  xN * om->xf[p7O_N][p7O_LOOP];
        xC = (xC * om->xf[p7O_C][p7O_LOOP]) +  (xE * om->xf[p7O_E][p7O_MOVE]);
        xJ = (xJ * om->xf[p7O_J][p7O_LOOP]) +  (xE * om->xf[p7O_E][p7O_LOOP]);
        xB = (xJ * om->xf[p7O_J][p7O_MOVE]) +  (xN * om->xf[p7O_N][p7O_MOVE]);
        /* and now xB will carry over into next i, and xC carries over after i=L */

        /* Sparse rescaling. xE above threshold? trigger a rescaling event.            */
        if (xE > 1.0e4)	/* that's a little less than e^10, ~10% of our dynamic range */
        {
            xN  = xN / xE;
            xC  = xC / xE;
            xJ  = xJ / xE;
            xB  = xB / xE;
            xEv = _mm_set1_ps(1.0 / xE);
            for (q = 0; q < Q; q++)
            {
                MMO(dpc,q) = _mm_mul_ps(MMO(dpc,q), xEv);
                DMO(dpc,q) = _mm_mul_ps(DMO(dpc,q), xEv);
                IMO(dpc,q) = _mm_mul_ps(IMO(dpc,q), xEv);
            }
            ox->xmx[i*p7X_NXCELLS+p7X_SCALE] = xE;
            ox->totscale += log((double)xE);
            xE = 1.0;		
        }
        else ox->xmx[i*p7X_NXCELLS+p7X_SCALE] = 1.0;

        /* Storage of the specials.  We could've stored these already
        * but using xE, etc. variables makes it easy to convert this
        * code to O(M) memory versions just by deleting storage steps.
        */
        ox->xmx[i*p7X_NXCELLS+p7X_E] = xE;
        ox->xmx[i*p7X_NXCELLS+p7X_N] = xN;
        ox->xmx[i*p7X_NXCELLS+p7X_J] = xJ;
        ox->xmx[i*p7X_NXCELLS+p7X_B] = xB;
        ox->xmx[i*p7X_NXCELLS+p7X_C] = xC;

#if p7_DEBUGGING
        if (ox->debugging) p7_omx_DumpFBRow(ox, TRUE, i, 9, 5, xE, xN, xJ, xB, xC);	/* logify=TRUE, <rowi>=i, width=8, precision=5*/
#endif
    } /* end loop over sequence residues 1..L */

    /* finally C->T, and flip total score back to log space (nats) */
    /* On overflow, xC is inf or nan (nan arises because inf*0 = nan). */
    /* On an underflow (which shouldn't happen), we counterintuitively return infinity:
    * the effect of this is to force the caller to rescore us with full range.
    */
    if       (isnan(xC))      ESL_EXCEPTION(eslERANGE, "forward score is NaN");
    else if  (L>0 && xC == 0.0)      ESL_EXCEPTION(eslERANGE, "forward score underflow (is 0.0)");
    else if  (isinf(xC) == 1) ESL_EXCEPTION(eslERANGE, "forward score overflow (is infinity)");

    if (opt_sc != NULL) *opt_sc = ox->totscale + log((double)(xC * om->xf[p7O_C][p7O_MOVE]));
    return eslOK;
}



static int 
backward_engine(int do_full, const ESL_DSQ *dsq, int L, const P7_OPROFILE *om, const P7_OMX *fwd, P7_OMX *bck, float *opt_sc,
                int percentBorder, U2::TaskStateInfo & ti)
{
    register __m128 mpv, ipv, dpv;      /* previous row values                                       */
    register __m128 mcv, dcv;           /* current row values                                        */
    register __m128 tmmv, timv, tdmv;   /* tmp vars for accessing rotated transition scores          */
    register __m128 xBv;		      /* collects B->Mk components of B(i)                         */
    register __m128 xEv;	              /* splatted E(i)                                             */
    __m128   zerov;		      /* splatted 0.0's in a vector                                */
    float    xN, xE, xB, xC, xJ;	      /* special states' scores                                    */
    int      i;			      /* counter over sequence positions 0,1..L                    */
    int      q;			      /* counter over quads 0..Q-1                                 */
    int      Q       = p7O_NQF(om->M);  /* segment length: # of vectors                              */
    int      j;			      /* DD segment iteration counter (4 = full serialization)     */
    __m128  *dpc;                       /* current DP row                                            */
    __m128  *dpp;			      /* next ("previous") DP row                                  */
  __m128  *rp;			      /* will point into om->rfv[x] for residue x[i+1]             */
  __m128  *tp;		              /* will point into (and step thru) om->tfv transition scores */

    /* initialize the L row. */
    bck->M = om->M;
    bck->L = L;
    bck->has_own_scales = FALSE;	/* backwards scale factors are *usually* given by <fwd> */
    dpc    = bck->dpf[L * do_full];
    xJ     = 0.0;
    xB     = 0.0;
    xN     = 0.0;
    xC     = om->xf[p7O_C][p7O_MOVE];      /* C<-T */
    xE     = xC * om->xf[p7O_E][p7O_MOVE]; /* E<-C, no tail */
    xEv    = _mm_set1_ps(xE); 
    zerov  = _mm_setzero_ps();  
    dcv    = zerov;		/* solely to silence a compiler warning */
    for (q = 0; q < Q; q++) MMO(dpc,q) = DMO(dpc,q) = xEv;
    for (q = 0; q < Q; q++) IMO(dpc,q) = zerov;

    /* init row L's DD paths, 1) first segment includes xE, from DMO(q) */
  tp  = om->tfv + 8*Q - 1;	                        /* <*tp> now the [4 8 12 x] TDD quad         */
    dpv = _mm_move_ss(DMO(dpc,Q-1), zerov);               /* start leftshift: [1 5 9 13] -> [x 5 9 13] */
    dpv = _mm_shuffle_ps(dpv, dpv, _MM_SHUFFLE(0,3,2,1)); /* finish leftshift:[x 5 9 13] -> [5 9 13 x] */
    for (q = Q-1; q >= 0; q--)
    {
        dcv        = _mm_mul_ps(dpv, *tp);      tp--;
        DMO(dpc,q) = _mm_add_ps(DMO(dpc,q), dcv);
        dpv        = DMO(dpc,q);
    }
    /* 2) three more passes, only extending DD component (dcv only; no xE contrib from DMO(q)) */
    for (j = 1; j < 4; j++)
    {
      tp  = om->tfv + 8*Q - 1;	                            /* <*tp> now the [4 8 12 x] TDD quad         */
        dcv = _mm_move_ss(dcv, zerov);                        /* start leftshift: [1 5 9 13] -> [x 5 9 13] */
        dcv = _mm_shuffle_ps(dcv, dcv, _MM_SHUFFLE(0,3,2,1)); /* finish leftshift:[x 5 9 13] -> [5 9 13 x] */
        for (q = Q-1; q >= 0; q--)
        {
            dcv        = _mm_mul_ps(dcv, *tp); tp--;
            DMO(dpc,q) = _mm_add_ps(DMO(dpc,q), dcv);
        }
    }
    /* now MD init */
  tp  = om->tfv + 7*Q - 3;	                        /* <*tp> now the [4 8 12 x] Mk->Dk+1 quad    */
    dcv = _mm_move_ss(DMO(dpc,0), zerov);                 /* start leftshift: [1 5 9 13] -> [x 5 9 13] */
    dcv = _mm_shuffle_ps(dcv, dcv, _MM_SHUFFLE(0,3,2,1)); /* finish leftshift:[x 5 9 13] -> [5 9 13 x] */
    for (q = Q-1; q >= 0; q--)
    {
        MMO(dpc,q) = _mm_add_ps(MMO(dpc,q), _mm_mul_ps(dcv, *tp)); tp -= 7;
        dcv        = DMO(dpc,q);
    }

    /* Sparse rescaling: same scale factors as fwd matrix */
    if (fwd->xmx[L*p7X_NXCELLS+p7X_SCALE] > 1.0)
    {
        xE  = xE / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
        xN  = xN / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
        xC  = xC / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
        xJ  = xJ / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
        xB  = xB / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
        xEv = _mm_set1_ps(1.0 / fwd->xmx[L*p7X_NXCELLS+p7X_SCALE]);
        for (q = 0; q < Q; q++) {
            MMO(dpc,q) = _mm_mul_ps(MMO(dpc,q), xEv);
            DMO(dpc,q) = _mm_mul_ps(DMO(dpc,q), xEv);
            IMO(dpc,q) = _mm_mul_ps(IMO(dpc,q), xEv);
        }
    }
    bck->xmx[L*p7X_NXCELLS+p7X_SCALE] = fwd->xmx[L*p7X_NXCELLS+p7X_SCALE];
    bck->totscale                     = log((double)bck->xmx[L*p7X_NXCELLS+p7X_SCALE]);

    /* Stores */
    bck->xmx[L*p7X_NXCELLS+p7X_E] = xE;
    bck->xmx[L*p7X_NXCELLS+p7X_N] = xN;
    bck->xmx[L*p7X_NXCELLS+p7X_J] = xJ;
    bck->xmx[L*p7X_NXCELLS+p7X_B] = xB;
    bck->xmx[L*p7X_NXCELLS+p7X_C] = xC;

#if p7_DEBUGGING
    if (bck->debugging) p7_omx_DumpFBRow(bck, TRUE, L, 9, 4, xE, xN, xJ, xB, xC);	/* logify=TRUE, <rowi>=L, width=9, precision=4*/
#endif

    /* main recursion */
    // ! ADDED CODE !
	int progressStart = ti.progress;
    for (i = L-1; i >= 1; i--)	/* backwards stride */
    {
        // ! ADDED CODE !
		ti.progress = progressStart + (int)(((double)percentBorder / L) * i);
        if( ti.cancelFlag ){ return eslCANCELED; }

        /* phase 1. B(i) collected. Old row destroyed, new row contains
        *    complete I(i,k), partial {MD}(i,k) w/ no {MD}->{DE} paths yet.
        */
        dpc = bck->dpf[i     * do_full];
        dpp = bck->dpf[(i+1) * do_full];
      rp  = om->rfv[dsq[i+1]] + Q-1; /* <*rp> is now the [4 8 12 x] match emission quad */
      tp  = om->tfv + 7*Q - 1;	     /* <*tp> is now the [4 8 12 x] TII transition quad  */

        /* leftshift the first transition quads */
      tmmv = _mm_move_ss(om->tfv[1], zerov); tmmv = _mm_shuffle_ps(tmmv, tmmv, _MM_SHUFFLE(0,3,2,1));
      timv = _mm_move_ss(om->tfv[2], zerov); timv = _mm_shuffle_ps(timv, timv, _MM_SHUFFLE(0,3,2,1));
      tdmv = _mm_move_ss(om->tfv[3], zerov); tdmv = _mm_shuffle_ps(tdmv, tdmv, _MM_SHUFFLE(0,3,2,1));

      mpv = _mm_mul_ps(MMO(dpp,0), om->rfv[dsq[i+1]][0]); /* precalc M(i+1,k+1) * e(M_k+1, x_{i+1}) */
        mpv = _mm_move_ss(mpv, zerov);
        mpv = _mm_shuffle_ps(mpv, mpv, _MM_SHUFFLE(0,3,2,1));

        xBv = zerov;
        for (q = Q-1; q >= 0; q--)     /* backwards stride */
        {
            ipv = IMO(dpp,q); /* assumes emission odds ratio of 1.0; i+1's IMO(q) now free */
            IMO(dpc,q) = _mm_add_ps(_mm_mul_ps(ipv, *tp), _mm_mul_ps(mpv, timv));   tp--;
            DMO(dpc,q) =                                  _mm_mul_ps(mpv, tdmv); 
            mcv        = _mm_add_ps(_mm_mul_ps(ipv, *tp), _mm_mul_ps(mpv, tmmv));   tp-= 2;

            mpv        = _mm_mul_ps(MMO(dpp,q), *rp);  rp--;  /* obtain mpv for next q. i+1's MMO(q) is freed  */
            MMO(dpc,q) = mcv;

            tdmv = *tp;   tp--;
            timv = *tp;   tp--;
            tmmv = *tp;   tp--;

            xBv = _mm_add_ps(xBv, _mm_mul_ps(mpv, *tp)); tp--;
        }

        /* phase 2: now that we have accumulated the B->Mk transitions in xBv, we can do the specials */
        /* this incantation is a horiz sum of xBv elements: (_mm_hadd_ps() would require SSE3) */
        xBv = _mm_add_ps(xBv, _mm_shuffle_ps(xBv, xBv, _MM_SHUFFLE(0, 3, 2, 1)));
        xBv = _mm_add_ps(xBv, _mm_shuffle_ps(xBv, xBv, _MM_SHUFFLE(1, 0, 3, 2)));
        _mm_store_ss(&xB, xBv);

        xC =  xC * om->xf[p7O_C][p7O_LOOP];
        xJ = (xB * om->xf[p7O_J][p7O_MOVE]) + (xJ * om->xf[p7O_J][p7O_LOOP]); /* must come after xB */
        xN = (xB * om->xf[p7O_N][p7O_MOVE]) + (xN * om->xf[p7O_N][p7O_LOOP]); /* must come after xB */
        xE = (xC * om->xf[p7O_E][p7O_MOVE]) + (xJ * om->xf[p7O_E][p7O_LOOP]); /* must come after xJ, xC */
        xEv = _mm_set1_ps(xE);	/* splat */


        /* phase 3: {MD}->E paths and one step of the D->D paths */
      tp  = om->tfv + 8*Q - 1;	/* <*tp> now the [4 8 12 x] TDD quad */
        dpv = _mm_add_ps(DMO(dpc,0), xEv);
        dpv = _mm_move_ss(dpv, zerov);
        dpv = _mm_shuffle_ps(dpv, dpv, _MM_SHUFFLE(0,3,2,1));
        for (q = Q-1; q >= 0; q--)
        {
            dcv        = _mm_mul_ps(dpv, *tp); tp--;
            DMO(dpc,q) = _mm_add_ps(DMO(dpc,q), _mm_add_ps(dcv, xEv));
            dpv        = DMO(dpc,q);
            MMO(dpc,q) = _mm_add_ps(MMO(dpc,q), xEv);
        }

        /* phase 4: finish extending the DD paths */
        /* fully serialized for now */
        for (j = 1; j < 4; j++)	/* three passes: we've already done 1 segment, we need 4 total */
        {
            dcv = _mm_move_ss(dcv, zerov);
            dcv = _mm_shuffle_ps(dcv, dcv, _MM_SHUFFLE(0,3,2,1));
	  tp  = om->tfv + 8*Q - 1;	/* <*tp> now the [4 8 12 x] TDD quad */
            for (q = Q-1; q >= 0; q--)
            {
                dcv        = _mm_mul_ps(dcv, *tp); tp--;
                DMO(dpc,q) = _mm_add_ps(DMO(dpc,q), dcv);
            }
        }

        /* phase 5: add M->D paths */
        dcv = _mm_move_ss(DMO(dpc,0), zerov);
        dcv = _mm_shuffle_ps(dcv, dcv, _MM_SHUFFLE(0,3,2,1));
      tp  = om->tfv + 7*Q - 3;	/* <*tp> is now the [4 8 12 x] Mk->Dk+1 quad */
        for (q = Q-1; q >= 0; q--)
        {
            MMO(dpc,q) = _mm_add_ps(MMO(dpc,q), _mm_mul_ps(dcv, *tp)); tp -= 7;
            dcv        = DMO(dpc,q);
        }

        /* Sparse rescaling  */

        /* In rare cases [J3/119] scale factors from <fwd> are
        * insufficient and backwards will overflow. In this case, we
        * switch on the fly to using our own scale factors, different
        * from those in <fwd>. This will complicate subsequent
        * posterior decoding routines.
        */
        if (xB > 1.0e16) bck->has_own_scales = TRUE;

        if      (bck->has_own_scales)  bck->xmx[i*p7X_NXCELLS+p7X_SCALE] = (xB > 1.0e4) ? xB : 1.0;
        else                           bck->xmx[i*p7X_NXCELLS+p7X_SCALE] = fwd->xmx[i*p7X_NXCELLS+p7X_SCALE];

        if (bck->xmx[i*p7X_NXCELLS+p7X_SCALE] > 1.0)
        {
            xE /= bck->xmx[i*p7X_NXCELLS+p7X_SCALE];
            xN /= bck->xmx[i*p7X_NXCELLS+p7X_SCALE];
            xJ /= bck->xmx[i*p7X_NXCELLS+p7X_SCALE];
            xB /= bck->xmx[i*p7X_NXCELLS+p7X_SCALE];
            xC /= bck->xmx[i*p7X_NXCELLS+p7X_SCALE];
            xBv = _mm_set1_ps(1.0 / bck->xmx[i*p7X_NXCELLS+p7X_SCALE]);
            for (q = 0; q < Q; q++) {
                MMO(dpc,q) = _mm_mul_ps(MMO(dpc,q), xBv);
                DMO(dpc,q) = _mm_mul_ps(DMO(dpc,q), xBv);
                IMO(dpc,q) = _mm_mul_ps(IMO(dpc,q), xBv);
            }
            bck->totscale += log((double)bck->xmx[i*p7X_NXCELLS+p7X_SCALE]);
        }

        /* Stores are separate only for pedagogical reasons: easy to
        * turn this into a more memory efficient version just by
        * deleting the stores.
        */
        bck->xmx[i*p7X_NXCELLS+p7X_E] = xE;
        bck->xmx[i*p7X_NXCELLS+p7X_N] = xN;
        bck->xmx[i*p7X_NXCELLS+p7X_J] = xJ;
        bck->xmx[i*p7X_NXCELLS+p7X_B] = xB;
        bck->xmx[i*p7X_NXCELLS+p7X_C] = xC;

#if p7_DEBUGGING
        if (bck->debugging) p7_omx_DumpFBRow(bck, TRUE, i, 9, 4, xE, xN, xJ, xB, xC);	/* logify=TRUE, <rowi>=i, width=9, precision=4*/
#endif
    } /* thus ends the loop over sequence positions i */

    /* Termination at i=0, where we can only reach N,B states. */
    dpp = bck->dpf[1 * do_full];
  tp  = om->tfv;          /* <*tp> is now the [1 5 9 13] TBMk transition quad  */
  rp  = om->rfv[dsq[1]];  /* <*rp> is now the [1 5 9 13] match emission quad   */
    xBv = zerov;
    for (q = 0; q < Q; q++)
    {
        mpv = _mm_mul_ps(MMO(dpp,q), *rp);  rp++;
        mpv = _mm_mul_ps(mpv,        *tp);  tp += 7;
        xBv = _mm_add_ps(xBv,        mpv);
    }
    /* horizontal sum of xBv */
    xBv = _mm_add_ps(xBv, _mm_shuffle_ps(xBv, xBv, _MM_SHUFFLE(0, 3, 2, 1)));
    xBv = _mm_add_ps(xBv, _mm_shuffle_ps(xBv, xBv, _MM_SHUFFLE(1, 0, 3, 2)));
    _mm_store_ss(&xB, xBv);

    xN = (xB * om->xf[p7O_N][p7O_MOVE]) + (xN * om->xf[p7O_N][p7O_LOOP]);  

    bck->xmx[p7X_B]     = xB;
    bck->xmx[p7X_C]     = 0.0;
    bck->xmx[p7X_J]     = 0.0;
    bck->xmx[p7X_N]     = xN;
    bck->xmx[p7X_E]     = 0.0;
    bck->xmx[p7X_SCALE] = 1.0;

#if p7_DEBUGGING
    dpc = bck->dpf[0];
    for (q = 0; q < Q; q++) /* Not strictly necessary, but if someone's looking at DP matrices, this is nice to do: */
        MMO(dpc,q) = DMO(dpc,q) = IMO(dpc,q) = zerov;
    if (bck->debugging) p7_omx_DumpFBRow(bck, TRUE, 0, 9, 4, bck->xmx[p7X_E], bck->xmx[p7X_N],  bck->xmx[p7X_J], bck->xmx[p7X_B],  bck->xmx[p7X_C]);	/* logify=TRUE, <rowi>=0, width=9, precision=4*/
#endif

    if       (isnan(xN))      ESL_EXCEPTION(eslERANGE, "backward score is NaN");
    else if  (L>0 && xN == 0.0)      ESL_EXCEPTION(eslERANGE, "backward score underflow (is 0.0)");
    else if  (isinf(xN) == 1) ESL_EXCEPTION(eslERANGE, "backward score overflow (is infinity)");

    if (opt_sc != NULL) *opt_sc = bck->totscale + log((double)xN);
    return eslOK;
}
/*-------------- end, forward/backward engines  -----------------*/

/************************************************************
* HMMER - Biological sequence analysis with profile HMMs
* Version 3.0; March 2010
* Copyright (C) 2010 Howard Hughes Medical Institute.
* Other copyrights also apply. See the COPYRIGHT file for a full list.
* 
* HMMER is distributed under the terms of the GNU General Public License
* (GPLv3). See the LICENSE file for details.
************************************************************/
