/* Implements the standard digitized alphabets for biosequences.
* 
*    1. ESL_ALPHABET object for digital alphabets.
*    2. Digitized sequences (ESL_DSQ *).
*    3. Other routines in the API.
*    7. Copyright notice and license.
* 
 * SVN $Id: esl_alphabet.c 393 2009-09-27 12:04:55Z eddys $
* SRE, Tue Dec  7 13:49:43 2004
*/

#include <hmmer3/easel/esl_config.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <hmmer3/easel/easel.h>
#include <hmmer3/easel/esl_alphabet.h>
#include <hmmer3/hmmer.h>

/*****************************************************************
* 1. The ESL_ALPHABET object
*****************************************************************/ 

static ESL_ALPHABET *create_rna(void);
static ESL_ALPHABET *create_dna(void);
static ESL_ALPHABET *create_amino(void);
static ESL_ALPHABET *create_coins(void);
static ESL_ALPHABET *create_dice(void);

/* Function:  esl_alphabet_Create()
* Synopsis:  Create alphabet of a standard type.
* Incept:    SRE, Mon Dec 20 10:21:54 2004 [Zaragoza]
*
* Purpose:   Creates one of the three standard bio alphabets:
*            <eslDNA>, <eslRNA>, or <eslAMINO>, and returns
*            a pointer to it.
*
* Args:      type  - <eslDNA>, <eslRNA>, or <eslAMINO>. 
*
* Returns:   pointer to the new alphabet.
*
* Throws:    <NULL> if any allocation or initialization fails.
*/
ESL_ALPHABET *
esl_alphabet_Create(int type)
{
    int           status;
    ESL_ALPHABET *a;

    switch(type) { 
  case eslRNA:    a = create_rna();   break;
  case eslDNA:    a = create_dna();   break;
  case eslAMINO:  a = create_amino(); break;
  case eslCOINS:  a = create_coins(); break;
  case eslDICE:   a = create_dice(); break;
  default:    
      ESL_XEXCEPTION(eslEINVAL, "bad alphabet type: unrecognized");
    }
    return a;

ERROR:
    return NULL;
}

/* Function:  esl_alphabet_CreateCustom()
* Synopsis:  Create a custom alphabet.
* Incept:    SRE, Mon Dec 20 09:18:28 2004 [Zaragoza]
*
* Purpose:   Creates a customized biosequence alphabet,
*            and returns a ptr to it. The alphabet type is set 
*            to <eslNONSTANDARD>.
*            
*            <alphabet> is the internal alphabet string;
*            <K> is the size of the base alphabet;
*            <Kp> is the total size of the alphabet string. 
*            
*            In the alphabet string, residues <0..K-1> are the base alphabet; 
*            residue <K> is the canonical gap (indel) symbol; 
*            residues <K+1..Kp-4> are additional degeneracy symbols (possibly 0 of them);
*            residue <Kp-3> is an "any" symbol (such as N or X); 
*            residue <Kp-2> is a "nonresidue" symbol (such as *); 
*            and residue <Kp-1> is a "missing data" gap symbol.
*            
*            The two gap symbols, the nonresidue, and the "any"
*            symbol are mandatory even for nonstandard alphabets, so
*            <Kp> $\geq$ <K+4>.
*            
* Args:      alphabet - internal alphabet; example "ACGT-RYMKSWHBVDN*~"
*            K        - base size; example 4
*            Kp       - total size, including gap, degeneracies; example 18
*
* Returns:   pointer to new <ESL_ALPHABET> structure.
*
* Throws:    <NULL> if any allocation or initialization fails.
*/
ESL_ALPHABET *
esl_alphabet_CreateCustom(const char *alphabet, int K, int Kp)
{
    ESL_ALPHABET *a;
    int           c,x,y;
    int           status;

    /* Argument checks.
    */
    if (strlen(alphabet) != Kp) ESL_XEXCEPTION(eslEINVAL, "alphabet length != Kp");
    if (Kp < K+4)               ESL_XEXCEPTION(eslEINVAL, "Kp too small in alphabet"); 

    /* Allocation/init, level 1.
    */
    ESL_ALLOC_WITH_TYPE(a, ESL_ALPHABET*, sizeof(ESL_ALPHABET));
    a->sym    = NULL;
    a->degen  = NULL;
    a->ndegen = NULL;

    /* Allocation/init, level 2.
    */
    ESL_ALLOC_WITH_TYPE(a->sym, char*,   sizeof(char)   * (Kp+1));
    ESL_ALLOC_WITH_TYPE(a->degen, char**,  sizeof(char *) * Kp);
    ESL_ALLOC_WITH_TYPE(a->ndegen, int*, sizeof(int)    * Kp);
    a->degen[0] = NULL;

    /* Allocation/init, level 3.
    */
    ESL_ALLOC_WITH_TYPE(a->degen[0], char*, sizeof(char) * (Kp*K));
    for (x = 1; x < Kp; x++)
        a->degen[x] = a->degen[0]+(K*x);

    /* Initialize the internal alphabet: 
    */
    a->type = eslNONSTANDARD;
    a->K    = K;
    a->Kp   = Kp;
    strcpy(a->sym, alphabet);

    /* Initialize the input map, mapping ASCII seq chars to digital codes,
    * and eslDSQ_ILLEGAL for everything else.
    */
    for (c = 0; c < 128; c++)   a->inmap[c]               = eslDSQ_ILLEGAL;
    for (x = 0; x < a->Kp; x++) a->inmap[(int) a->sym[x]] = x;  

    /* Initialize the degeneracy map:
    *  Base alphabet (first K syms) are automatically
    *  mapped uniquely; (Kp-3) is assumed to be
    *  the "any" character; other degen chars (K+1..Kp-4) are 
    *  unset; gap, nonresidue, missing character are unmapped (ndegen=0)
    */
    for (x = 0; x < a->Kp; x++)  	/* clear everything */
    {
        a->ndegen[x] = 0;
        for (y = 0; y < a->K; y++) a->degen[x][y] = 0;
    }
    for (x = 0; x < a->K; x++) 	/* base alphabet */
    {
        a->ndegen[x]   = 1;
        a->degen[x][x] = 1;
    }
    /* "any" character */
    a->ndegen[Kp-3]  = K;
    for (x = 0; x < a->K; x++) a->degen[Kp-3][x] = 1;

    a->complement = NULL;
    return a;

ERROR:
    esl_alphabet_Destroy(a);
    return NULL;
}


/* define_complementarity()
* Builds the "complement" lookup table for DNA, RNA alphabets.
*/
static int
define_complementarity(ESL_ALPHABET *a)
{
    int  status;

    ESL_ALLOC_WITH_TYPE(a->complement, ESL_DSQ*, sizeof(ESL_DSQ) * a->Kp);
    a->complement[0] = 3;	   /* A->T */
    a->complement[1] = 2;    /* C->G */
    a->complement[2] = 1;    /* G->C */
    a->complement[3] = 0;    /* T->A */
    a->complement[4] = 4;    /* -  - */
    a->complement[5] = 6;	   /* R->Y */
    a->complement[6] = 5;    /* Y->R */
    a->complement[7] = 8;    /* M->K */
    a->complement[8] = 7;    /* K->M */
    a->complement[9] = 9;    /* S  S */
    a->complement[10]= 10;   /* W  W */
    a->complement[11]= 14;   /* H->D */
    a->complement[12]= 13;   /* B->V */
    a->complement[13]= 12;   /* V->B */
    a->complement[14]= 11;   /* D->H */
    a->complement[15]= 15;   /* N  N */
    a->complement[16]= 16;   /* ~  ~ */
    return eslOK;

ERROR:
    return status;
}

/* create_rna(): 
* Creates a standard RNA alphabet.
*/
static ESL_ALPHABET *
create_rna(void)
{
    ESL_ALPHABET *a = NULL;

    /* Create the fundamental alphabet
    */
    if ((a = esl_alphabet_CreateCustom("ACGU-RYMKSWHBVDN*~", 4, 18)) == NULL) return NULL;
    a->type = eslRNA;

    /* Add desired synonyms in the input map.
    */
    esl_alphabet_SetEquiv(a, 'T', 'U');	    /* read T as a U */
    esl_alphabet_SetEquiv(a, 'X', 'N');	    /* read X as an N (many seq maskers use X) */
    esl_alphabet_SetEquiv(a, '_', '-');       /* allow _ as a gap too */
    esl_alphabet_SetEquiv(a, '.', '-');       /* allow . as a gap too */
    esl_alphabet_SetCaseInsensitive(a);       /* allow lower case input */

    /* Define degenerate symbols.
    */
    esl_alphabet_SetDegeneracy(a, 'R', "AG");
    esl_alphabet_SetDegeneracy(a, 'Y', "CU");
    esl_alphabet_SetDegeneracy(a, 'M', "AC");
    esl_alphabet_SetDegeneracy(a, 'K', "GU");
    esl_alphabet_SetDegeneracy(a, 'S', "CG");
    esl_alphabet_SetDegeneracy(a, 'W', "AU");
    esl_alphabet_SetDegeneracy(a, 'H', "ACU");
    esl_alphabet_SetDegeneracy(a, 'B', "CGU");
    esl_alphabet_SetDegeneracy(a, 'V', "ACG");
    esl_alphabet_SetDegeneracy(a, 'D', "AGU");  

    if (define_complementarity(a) != eslOK) return NULL;

    return a;
}


/* create_dna(): 
* creates and returns a standard DNA alphabet.
*/
static ESL_ALPHABET *
create_dna(void)
{
    ESL_ALPHABET *a = NULL;

    /* Create the fundamental alphabet.
    */
    if ((a = esl_alphabet_CreateCustom("ACGT-RYMKSWHBVDN*~", 4, 18)) == NULL) return NULL;
    a->type = eslDNA;

    /* Add desired synonyms in the input map.
    */
    esl_alphabet_SetEquiv(a, 'U', 'T');	    /* read U as a T */
    esl_alphabet_SetEquiv(a, 'X', 'N');	    /* read X as an N (many seq maskers use X) */
    esl_alphabet_SetEquiv(a, '_', '-');       /* allow _ as a gap too */
    esl_alphabet_SetEquiv(a, '.', '-');       /* allow . as a gap too */
    esl_alphabet_SetCaseInsensitive(a);       /* allow lower case input */

    /* Define IUBMB degenerate symbols other than the N.
    */
    esl_alphabet_SetDegeneracy(a, 'R', "AG");
    esl_alphabet_SetDegeneracy(a, 'Y', "CT");
    esl_alphabet_SetDegeneracy(a, 'M', "AC");
    esl_alphabet_SetDegeneracy(a, 'K', "GT");
    esl_alphabet_SetDegeneracy(a, 'S', "CG");
    esl_alphabet_SetDegeneracy(a, 'W', "AT");
    esl_alphabet_SetDegeneracy(a, 'H', "ACT");
    esl_alphabet_SetDegeneracy(a, 'B', "CGT");
    esl_alphabet_SetDegeneracy(a, 'V', "ACG");
    esl_alphabet_SetDegeneracy(a, 'D', "AGT");  

    if (define_complementarity(a) != eslOK) return NULL;
    return a;
}


/* create_amino():
* Creates a new standard amino acid alphabet.
*/
static ESL_ALPHABET *
create_amino(void)
{
    ESL_ALPHABET *a = NULL;

    /* Create the internal alphabet
    */
    if ((a = esl_alphabet_CreateCustom("ACDEFGHIKLMNPQRSTVWY-BJZOUX*~", 20, 29)) == NULL) return NULL;
    a->type = eslAMINO;

    /* Add desired synonyms in the input map.
    */
    esl_alphabet_SetEquiv(a, '_', '-');       /* allow _ as a gap too */
    esl_alphabet_SetEquiv(a, '.', '-');       /* allow . as a gap too */
    esl_alphabet_SetCaseInsensitive(a);       /* allow lower case input */

    /* Define IUPAC degenerate symbols other than the X.
    */
    esl_alphabet_SetDegeneracy(a, 'B', "ND");
    esl_alphabet_SetDegeneracy(a, 'J', "IL");
    esl_alphabet_SetDegeneracy(a, 'Z', "QE");

    /* Define unusual residues as one-to-one degeneracies.
    */
    esl_alphabet_SetDegeneracy(a, 'U', "C"); /* selenocysteine is scored as cysteine */
    esl_alphabet_SetDegeneracy(a, 'O', "K"); /* pyrrolysine is scored as lysine      */

    return a;
}


/* create_coins():
* Creates a toy alphabet for coin examples
*/
static ESL_ALPHABET *
create_coins(void)
{
    ESL_ALPHABET *a = NULL;

    /* Create the internal alphabet
    */
    if ((a = esl_alphabet_CreateCustom("HT-X*~", 2, 6)) == NULL) return NULL;
    a->type = eslCOINS;

    /* Add desired synonyms in the input map.
    */
    esl_alphabet_SetEquiv(a, '_', '-');       /* allow _ as a gap too */
    esl_alphabet_SetEquiv(a, '.', '-');       /* allow . as a gap too */
    esl_alphabet_SetCaseInsensitive(a);       /* allow lower case input */

    /* There are no degeneracies in the coin alphabet. */

    return a;
}

/* create_dice():
* Creates a toy alphabet for dice examples
*/
static ESL_ALPHABET *
create_dice(void)
{
    ESL_ALPHABET *a = NULL;

    /* Create the internal alphabet
    */
    if ((a = esl_alphabet_CreateCustom("123456-X*~", 6, 10)) == NULL) return NULL;
    a->type = eslCOINS;

    /* Add desired synonyms in the input map.
    */
    esl_alphabet_SetEquiv(a, '_', '-');       /* allow _ as a gap too */
    esl_alphabet_SetEquiv(a, '.', '-');       /* allow . as a gap too */
    esl_alphabet_SetCaseInsensitive(a);       /* allow lower case input */

    /* There are no degeneracies in the dice alphabet. */

    return a;
}



/* Function:  esl_alphabet_SetEquiv()
* Synopsis:  Define an equivalent symbol.
* Incept:    SRE, Mon Dec 20 10:40:33 2004 [Zaragoza]
*
* Purpose:   Maps an additional input alphabetic symbol <sym> to 
*            an internal alphabet symbol <c>; for example,
*            we might map T to U for an RNA alphabet, so that we
*            allow for reading input DNA sequences.
*            
* Args:      sym   - symbol to allow in the input alphabet; 'T' for example
*            c     - symbol to map <sym> to in the internal alphabet; 'U' for example
*
* Returns:   <eslOK> on success.
*
* Throws:    <eslEINVAL> if <c> is not in the internal alphabet, or if <sym> is.
*/
int
esl_alphabet_SetEquiv(ESL_ALPHABET *a, char sym, char c)
{
    char    *sp = NULL;
    ESL_DSQ  x;

    /* Contract checks */
    if ((sp = strchr(a->sym, sym)) != NULL)
        ESL_EXCEPTION(eslEINVAL, "symbol %c is already in internal alphabet, can't equivalence it", sym);
    if ((sp = strchr(a->sym, c)) == NULL) 
        ESL_EXCEPTION(eslEINVAL, "char %c not in the alphabet, can't map to it", c);

    x = sp - a->sym;
    a->inmap[(int) sym] = x;
    return eslOK;
}

/* Function:  esl_alphabet_SetCaseInsensitive()
* Synopsis:  Make an alphabet's input map case-insensitive.
* Incept:    SRE, Mon Dec 20 15:31:12 2004 [Zaragoza]
*
* Purpose:   Given a custom alphabet <a>, with all equivalences set,
*            make the input map case-insensitive: for every
*            letter that is mapped in either lower or upper
*            case, map the other case to the same internal
*            residue.
*
*            For the standard alphabets, this is done automatically.
*
* Args:      a  - alphabet to make case-insensitive.
*                 
* Returns:   <eslOK> on success.                
* 
* Throws:    <eslECORRUPT> if any lower/uppercase symbol pairs
*            are already both mapped to different symbols.
*/
int
esl_alphabet_SetCaseInsensitive(ESL_ALPHABET *a)
{
    int lc, uc;

    for (lc = 'a'; lc <= 'z'; lc++)
    {
        uc = toupper(lc);

        if      (esl_abc_CIsValid(a, lc) && ! esl_abc_CIsValid(a, uc)) a->inmap[uc] = a->inmap[lc];
        else if (esl_abc_CIsValid(a, uc) && ! esl_abc_CIsValid(a, lc)) a->inmap[lc] = a->inmap[uc];
        else if (esl_abc_CIsValid(a, lc) && esl_abc_CIsValid(a, uc) && a->inmap[uc] != a->inmap[lc])
            ESL_EXCEPTION(eslECORRUPT, "symbols %c and %c map differently already (%c vs. %c)",
            lc, uc, a->inmap[lc], a->inmap[uc]);
    }
    return eslOK;
}

/* Function:  esl_alphabet_SetDegeneracy()
* Synopsis:  Define degenerate symbol in custom alphabet.
* Incept:    SRE, Mon Dec 20 15:42:23 2004 [Zaragoza]
*
* Purpose:   Given an alphabet under construction, 
*            define the degenerate character <c> to mean
*            any of the characters in the string <ds>.
*
*            <c> must exist in the digital alphabet, as
*            one of the optional degenerate residues (<K+1>..<Kp-3>).
*            All the characters in the <ds> string must exist
*            in the canonical alphabet (<0>..<K-1>).
*            
*            You may not redefine the mandatory all-degenerate character
*            (typically <N> or <X>; <Kp-3> in the digital alphabet).
*            It is defined automatically in all alphabets. 
*
* Args:      a   - an alphabet under construction.
*            c   - degenerate character code; example: 'R'
*            ds  - string of base characters for c; example: "AG"
*
* Returns:   <eslOK> on success.
*
* Throws:    <eslEINVAL> if <c> or <ds> arguments aren't valid.
*/
int
esl_alphabet_SetDegeneracy(ESL_ALPHABET *a, char c, char *ds)
{
    char   *sp;
    ESL_DSQ x,y;

    if ((sp = strchr(a->sym, c)) == NULL)
        ESL_EXCEPTION(eslEINVAL, "no such degenerate character");
    x = sp - a->sym;

    /* A degenerate character must have code K+1..Kp-4.
    * Kp-3, the all-degenerate character, is automatically
    * created, and can't be remapped.
    */
    if (x == a->Kp-3) 
        ESL_EXCEPTION(eslEINVAL, "can't redefine all-degenerate char %c", c);
    if (x < a->K+1 || x >= a->Kp-2) 
        ESL_EXCEPTION(eslEINVAL, "char %c isn't in expected position in alphabet", c);

    while (*ds != '\0') {
        if ((sp = strchr(a->sym, *ds)) == NULL) ESL_EXCEPTION(eslEINVAL, "no such base character");
        y = sp - a->sym;
        if (! esl_abc_XIsCanonical(a, y))       ESL_EXCEPTION(eslEINVAL, "can't map degeneracy to noncanonical character");

        a->degen[x][y] = 1;
        a->ndegen[x]++;
        ds++;
    }
    return eslOK;
}


/* Function:  esl_alphabet_SetIgnored()
* Synopsis:  Define a set of characters to be ignored in input.
* Incept:    SRE, Tue Sep 19 15:08:27 2006 [Janelia]
*
* Purpose:   Given an alphabet <a> (either standard or custom), define
*            all the characters in string <ignoredchars> to be
*            unmapped: valid, but ignored when converting input text.
*            
*            By default, the standard alphabets do not define any
*            ignored characters.
*            
*            The most common ignored characters would be space, tab,
*            and digits, to skip silently over whitespace and
*            sequence coordinates when parsing loosely-defined
*            sequence file formats.
*
* Args:      a             - alphabet to modify
*            ignoredchars  - string listing characters to ignore; i.e. " \t"
*
* Returns:   <eslOK> on success.
*/
int
esl_alphabet_SetIgnored(ESL_ALPHABET *a, const char *ignoredchars)
{
    int i;
    for (i = 0; ignoredchars[i] != '\0'; i++) a->inmap[(int)ignoredchars[i]] = eslDSQ_IGNORED;
    return eslOK;
}


/* Function:  esl_alphabet_Destroy()
* Synopsis:  Frees an alphabet object.
* Incept:    SRE, Mon Dec 20 10:27:23 2004 [Zaragoza]
*
* Purpose:   Free's an <ESL_ALPHABET> structure.
*
* Args:      a  - the <ESL_ALPHABET> to free.
*
* Returns:   (void).
*/
void
esl_alphabet_Destroy(ESL_ALPHABET *a)
{
    if (a == NULL) return;

    if (a->sym      != NULL) free(a->sym);
    if (a->ndegen   != NULL) free(a->ndegen);
    if (a->degen    != NULL) 
    {
        if (a->degen[0] != NULL) free(a->degen[0]);
        free(a->degen);
    }
    if (a->complement != NULL) free(a->complement);
    free(a);
}
/*--------------- end, ESL_ALPHABET object ----------------------*/





/*****************************************************************
* 2. Digitized sequences (ESL_DSQ *)
*****************************************************************/ 
/* Design note:                 SRE, Mon Sep 18 09:11:41 2006
* 
* An ESL_DSQ is considered to a special string type, equivalent to
* <char *>, and is not considered to be an Easel "object".  Thus it
* does not have a standard object API.  Rather, the caller deals with
* an ESL_DSQ directly: allocate for <(L+2)*sizeof(ESL_DSQ)> to leave
* room for sentinels at <0> and <L+1>.  
* 
* Additionally, an ESL_DSQ is considered to be "trusted"
* data: we're 'guaranteed' that anything in an ESL_DSQ is a valid
* symbol, so we don't need to error-check. Anything else is a programming
* error.
*/

/* Function:  esl_abc_CreateDsq()
* Synopsis:  Digitizes a sequence into new space.
* Incept:    SRE, Mon Sep 18 09:15:02 2006 [Janelia]
*
* Purpose:   Given an alphabet <a> and an ASCII sequence <seq>,
*            digitize the sequence into newly allocated space, and 
*            return a pointer to that space in <ret_dsq>.
*            
* Args:      a       - internal alphabet
*            seq     - text sequence to be digitized
*            ret_dsq - RETURN: the new digital sequence
*
* Returns:   <eslOK> on success, and <ret_dsq> contains the digitized
*            sequence; caller is responsible for free'ing this
*            memory. Returns <eslEINVAL> if <seq> contains
*            one or more characters that are not in the input map of
*            alphabet <a>. If this happens, <ret_dsq> is still valid upon
*            return: invalid characters are replaced by full ambiguities
*            (typically X or N).
*
* Throws:    <eslEMEM> on allocation failure.
*
* Xref:      STL11/63
*/
int
esl_abc_CreateDsq(const ESL_ALPHABET *a, const char *seq, ESL_DSQ **ret_dsq)
{
    ESL_DSQ *dsq = NULL;
    int      status;
    int64_t  L;

    L = strlen(seq);
    ESL_ALLOC_WITH_TYPE(dsq, ESL_DSQ*, sizeof(ESL_DSQ) * (L+2));
    status = esl_abc_Digitize(a, seq, dsq);

    if (ret_dsq != NULL) *ret_dsq = dsq; else free(dsq);
    return status;

ERROR:
    if (dsq != NULL)      free(dsq);
    if (ret_dsq != NULL) *ret_dsq = NULL;
    return status;
}


/* Function: esl_abc_Digitize()
* Synopsis: Digitizes a sequence into existing space.
* Incept:   SRE, Sun Aug 27 11:18:56 2006 [Leesburg]
* 
* Purpose:  Given an alphabet <a> and a nul-terminated ASCII sequence
*           <seq>, digitize the sequence and put it in <dsq>. Caller
*           provides space in <dsq> allocated for at least <L+2>
*           <ESL_DSQ> residues, where <L> is the length of <seq>.
*           
* Args:     a       - internal alphabet
*           seq     - text sequence to be digitized (\0-terminated)
*           dsq     - RETURN: the new digital sequence (caller allocates,
*                     at least <(L+2) * sizeof(ESL_DSQ)>).
*           
* Returns:  <eslOK> on success.
*           Returns <eslEINVAL> if <seq> contains one or more characters
*           that are not recognized in the alphabet <a>. (This is classed
*           as a normal error, because the <seq> may be untrusted user input.)
*           If this happens, the digital sequence <dsq> is still valid upon
*           return; invalid ASCII characters are replaced by ambiguities
*           (X or N).
*/
int
esl_abc_Digitize(const ESL_ALPHABET *a, const char *seq, ESL_DSQ *dsq)
{
    int     status;
    int64_t i;			/* position in seq */
    int64_t j;			/* position in dsq */
    ESL_DSQ x;

    status = eslOK;
    dsq[0] = eslDSQ_SENTINEL;
    for (i = 0, j = 1; seq[i] != '\0'; i++) 
    { 
        x = a->inmap[(int) seq[i]];
        if (x == eslDSQ_IGNORED) continue; 

        if (esl_abc_XIsValid(a, x))
            dsq[j] = x;
        else
        {
            status   = eslEINVAL;
            dsq[j] = esl_abc_XGetUnknown(a);
        }
        j++;
    }
    dsq[j] = eslDSQ_SENTINEL;
    return status;
}

/* Function:  esl_abc_Textize()
* Synopsis:  Convert digital sequence to text.
* Incept:    SRE, Sun Aug 27 11:14:58 2006 [Leesburg]
*
* Purpose:   Make an ASCII sequence <seq> by converting a digital
*            sequence <dsq> of length <L> back to text, according to
*            the digital alphabet <a>. 
*            
*            Caller provides space in <seq> allocated for at least
*            <L+1> bytes (<(L+1) * sizeof(char)>).
*
* Args:      a   - internal alphabet
*            dsq - digital sequence to be converted (1..L)
*            L   - length of dsq
*            seq - RETURN: the new text sequence (caller allocated
*                  space, at least <(L+1) * sizeof(char)>).
*            
* Returns:   <eslOK> on success.
*/
int
esl_abc_Textize(const ESL_ALPHABET *a, const ESL_DSQ *dsq, int64_t L, char *seq)
{
    int64_t i;

    for (i = 0; i < L; i++)
        seq[i] = a->sym[dsq[i+1]];
    seq[i] = '\0';
    return eslOK;
}


/* Function:  esl_abc_TextizeN()
* Synopsis:  Convert subsequence from digital to text.
* Incept:    SRE, Tue Sep  5 09:28:38 2006 [Janelia] STL11/54.
*
* Purpose:   Similar in semantics to <strncpy()>, this procedure takes
*            a window of <L> residues in a digitized sequence
*            starting at the residue pointed to by <dptr>,
*            converts them to ASCII text representation, and 
*            copies them into the buffer <buf>.
*            
*            <buf> must be at least <L> residues long; <L+1>, if the
*            caller needs to NUL-terminate it.
*            
*            If a sentinel byte is encountered in the digitized
*            sequence before <L> residues have been copied, <buf> is
*            NUL-terminated there. Otherwise, like <strncpy()>, <buf>
*            will not be NUL-terminated.
*            
*            Note that because digital sequences are indexed <1..N>,
*            not <0..N-1>, the caller must be careful about
*            off-by-one errors in <dptr>. For example, to copy from
*            the first residue of a digital sequence <dsq>, you must
*            pass <dptr=dsq+1>, not <dptr=dsq>. The text in <buf>
*            on the other hand is a normal C string indexed <0..L-1>.
*
* Args:      a     - reference to an internal alphabet
*            dptr  - ptr to starting residue in a digital sequence
*            L     - number of residues to convert and copy
*            buf   - text buffer to store the <L> converted residues in
*
* Returns:   <eslOK> on success.
*/
int
esl_abc_TextizeN(const ESL_ALPHABET *a, const ESL_DSQ *dptr, int64_t L, char *buf)
{
    int64_t i;

    for (i = 0; i < L; i++)
    {
        if (dptr[i] == eslDSQ_SENTINEL) 
        { 
            buf[i] = '\0';
            return eslOK;
        }
        buf[i] = a->sym[dptr[i]];
    }
    return eslOK;
}


/* Function:  esl_abc_dsqcpy()
* Incept:    SRE, Fri Feb 23 08:45:10 2007 [Casa de Gatos]
*
* Purpose:   Given a digital sequence <dsq> of length <L>,
*            make a copy of it in <dcopy>. Caller provides
*            storage in <dcopy> for at least <L+2> <ESL_DSQ>
*            residues.
*
* Returns:   <eslOK> on success.
*/
int
esl_abc_dsqcpy(const ESL_DSQ *dsq, int64_t L, ESL_DSQ *dcopy)
{
    memcpy(dcopy, dsq, sizeof(ESL_DSQ) * (L+2));
    return eslOK;
}


/* Function:  esl_abc_dsqdup()
* Synopsis:  Duplicate a digital sequence.
* Incept:    SRE, Tue Aug 29 13:51:05 2006 [Janelia]
*
* Purpose:   Like <esl_strdup()>, but for digitized sequences:
*            make a duplicate of <dsq> and leave it in <ret_dup>.
*            Caller can pass the string length <L> if it's known, saving
*            some overhead; else pass <-1> and the length will be
*            determined for you.
*            
*            Tolerates <dsq> being <NULL>; in which case, returns
*            <eslOK> with <*ret_dup> set to <NULL>.
*
* Args:      dsq     - digital sequence to duplicate (w/ sentinels at 0,L+1)
*            L       - length of dsq in residues, if known; -1 if unknown
*            ret_dup - RETURN: allocated duplicate of <dsq>, which caller will
*                      free.
*
* Returns:   <eslOK> on success, and leaves a pointer in <ret_dup>.
*
* Throws:    <eslEMEM> on allocation failure.
*
* Xref:      STL11/48
*/
int 
esl_abc_dsqdup(const ESL_DSQ *dsq, int64_t L, ESL_DSQ **ret_dup)
{
    int      status;
    ESL_DSQ *newSq = NULL;

    if (ret_dup == NULL) return eslOK; /* no-op. */

    *ret_dup = NULL;
    if (dsq == NULL) return eslOK;
    if (L < 0) L = esl_abc_dsqlen(dsq);

    ESL_ALLOC_WITH_TYPE(newSq, ESL_DSQ*, sizeof(ESL_DSQ) * (L+2));
    memcpy(newSq, dsq, sizeof(ESL_DSQ) * (L+2));

    *ret_dup = newSq;
    return eslOK;

ERROR:
    if (newSq     != NULL)  free(newSq);
    if (ret_dup != NULL) *ret_dup = NULL;
    return status;
}


/* Function:  esl_abc_dsqcat()
* Synopsis:  Concatenate digital sequences.
* Incept:    SRE, Tue Aug 29 14:01:59 2006 [Janelia]
*
* Purpose:   Like <esl_strcat()>, except specialized for digitizing a
*            biosequence text string and appending it to a growing
*            digital sequence. The growing digital sequence is <dsq>,
*            currently of length <L> residues; we append <s> to it,
*            of length <n> symbols, after digitization.  Upon return,
*            <dsq> has been reallocated and <L> is set to the new
*            length (which is why both must be passed by reference).
*            
*            Note that the final <L> is not necessarily the initial
*            <L> plus <n>, because the text string <s> may contain
*            symbols that are defined to be ignored
*            (<eslDSQ_IGNORED>) in the input map of this alphabet.
*            (The final <L> is guaranteed to be $\leq$ <L+n> though.>
*            
*            If the initial <L> is unknown, pass <-1>, and it will be
*            determined by counting the residues in <dsq>.
*            
*            Similarly, if <n> is unknown, pass <-1> and it will be
*            determined by counting the symbols in <s>
*            
*            <dsq> may be <NULL>, in which case this call is
*            equivalent to an allocation and digitization just of
*            <s>.
*            
*            <s> may also be <NULL>, in which case <dsq> is
*            unmodified; <L> would be set to the correct length of
*            <dsq> if it was passed as <-1> (unknown).
*            
* Args:      abc  - digital alphabet to use
*            dsq  - reference to the current digital seq to append to 
*                   (with sentinel bytes at 0,L+1); may be <NULL>. 
*                   Upon return, this will probably have 
*                   been reallocated, and it will contain the original
*                   <dsq> with <s> digitized and appended.
*            L    - reference to the current length of <dsq> in residues;
*                   may be <-1> if unknown. Upon return, <L> is set to
*                   the new length of <dsq>, after <s> is appended.
*            s    - NUL-terminated ASCII text sequence to append. May
*                   contain ignored text characters (flagged with
*                   <eslDSQ_IGNORED> in the input map of alphabet <abc>).  
*            n    - Length of <s> in characters, if known; or <-1> if 
*                   unknown.
*
* Returns:   <eslOK> on success; <dsq> contains the result of digitizing
*            and appending <s> to the original <dsq>; and <L> contains
*            the new length of the <dsq> result in residues.
*            
*            If any of the characters in <s> are illegal in the alphabet
*            <abc>, these characters are digitized as unknown residues, 
*            and the function returns <eslEINVAL>. The caller might want
*            to call <esl_abc_ValidateSeq()> on <s> if it wants to figure
*            out where digitization goes awry and get a more informative
*            error report. This is a normal error, because the string <s>
*            might be user input.
*
* Throws:    <eslEMEM> on allocation or reallocation failure;
*
* Xref:      STL11/48.
*/
int
esl_abc_dsqcat(const ESL_ALPHABET *a, ESL_DSQ **dsq, int64_t *L, const char *s, int64_t n)
{
    int     status;
    void   *p;
    int64_t newL;
    int64_t xpos, cpos;
    ESL_DSQ x;

    if (*L < 0) newL = ((*dsq == NULL) ? 0 : esl_abc_dsqlen(*dsq));
    else        newL = *L;

    if (n < 0)  n = ((s == NULL) ? 0 : strlen(s));

    /* below handles weird case of empty s (including empty dsq and empty s):
    * just hand dsq and its length right back to the caller.
    */
    if (n == 0) { *L = newL; return eslOK; } 

    if (*dsq == NULL) {		/* an entirely new dsq must be allocated *and* initialized with left sentinel. */
        ESL_ALLOC_WITH_TYPE(*dsq, ESL_DSQ*, sizeof(ESL_DSQ)     * (n+2));
        (*dsq)[0] = eslDSQ_SENTINEL;
    } else			/* else, existing dsq is just reallocated; left sentinel already in place. */
        ESL_RALLOC_WITH_TYPE(*dsq, ESL_DSQ*, p, sizeof(ESL_DSQ) * (newL+n+2)); /* most we'll need */

    /* Watch these coords. Start in the 0..n-1 text string at 0;
    * start in the 1..L dsq at L+1, overwriting its terminal 
    * sentinel byte.
    */
    status = eslOK;
    for (xpos = newL+1, cpos = 0; s[cpos] != '\0'; cpos++)
    {
        x = a->inmap[(int) s[cpos]];
        if (esl_abc_XIsValid(a, x))
            (*dsq)[xpos++] = x;
        else if (x == eslDSQ_IGNORED)
            ;
        else 
        {
            (*dsq)[xpos++] = esl_abc_XGetUnknown(a);
            status = eslEINVAL;
        }
    }
    (*dsq)[xpos] = eslDSQ_SENTINEL;
    *L = xpos-1;
    return status;

ERROR:
    *L = newL;
    return status;
}

/* Function:  esl_abc_dsqlen()
* Synopsis:  Returns the length of a digital sequence.
* Incept:    SRE, Tue Aug 29 13:49:02 2006 [Janelia]
*
* Purpose:   Returns the length of digitized sequence <dsq> in
*            positions (including gaps, if any). The <dsq> must be
*            properly terminated by a sentinel byte
*            (<eslDSQ_SENTINEL>).  
*/
int64_t 
esl_abc_dsqlen(const ESL_DSQ *dsq)
{
    int64_t n = 0;
    while (dsq[n+1] != eslDSQ_SENTINEL) n++;
    return n;
}

/* Function:  esl_abc_dsqrlen()
* Synopsis:  Returns the number of residues in a digital seq.
* Incept:    SRE, Sat Nov  4 09:41:40 2006 [Janelia]
*
* Purpose:   Returns the unaligned length of digitized sequence
*            <dsq>, in residues, not counting any gaps or
*            missing data symbols. 
*/
int64_t
esl_abc_dsqrlen(const ESL_ALPHABET *abc, const ESL_DSQ *dsq)
{
    int64_t n = 0;
    int64_t i;

    for (i = 1; dsq[i] != eslDSQ_SENTINEL; i++)
        if (esl_abc_XIsResidue(abc, dsq[i])) n++;
    return n;
}

/* Function:  esl_abc_CDealign()
* Synopsis:  Dealigns a text string, using a reference digital aseq.
* Incept:    SRE, Sun Mar 30 13:14:05 2008 [Casa de Gatos]
*
* Purpose:   Dealigns <s> in place by removing characters aligned to
*            gaps (or missing data symbols) in the reference digital
*            aligned sequence <ref_ax>. Gaps and missing data symbols
*            in <ref_ax> are defined by its digital alphabet <abc>.
*            
*            <s> is typically going to be some kind of textual
*            annotation string (secondary structure, consensus, or
*            surface accessibility).
*            
*            Be supercareful of off-by-one errors here! The <ref_ax>
*            is a digital sequence that is indexed <1..L>. The
*            annotation string <s> is assumed to be <0..L-1> (a
*            normal C string), off by one with respect to <ref_ax>.
*            In a sequence object, ss annotation is actually stored
*            <1..L> -- so if you're going to <esl_abc_CDealign()> a
*            <sq->ss>, pass <sq->ss+1> as the argument <s>.
*
* Returns:   Returns <eslOK> on success; optionally returns the number
*            of characters in the dealigned <s> in <*opt_rlen>.
*
* Throws:    (no abnormal error conditions)
*/
int
esl_abc_CDealign(const ESL_ALPHABET *abc, char *s, const ESL_DSQ *ref_ax, int64_t *opt_rlen)
{
    int64_t apos;
    int64_t n = 0;

    if (s == NULL) return eslOK;

    for (n=0, apos=1; ref_ax[apos] != eslDSQ_SENTINEL; apos++)
        if (! esl_abc_XIsGap(abc, ref_ax[apos]) && ! esl_abc_XIsMissing(abc, ref_ax[apos]) )
            s[n++] = s[apos-1];	/* apos-1 because we assume s was 0..alen-1, whereas ref_ax was 1..alen */
    s[n] = '\0';

    if (opt_rlen != NULL) *opt_rlen = n;
    return eslOK;
}

/* Function:  esl_abc_XDealign()
* Synopsis:  Dealigns a digital string, using a reference digital aseq.
* Incept:    SRE, Sun Mar 30 13:19:16 2008 [Casa de Gatos]
*
* Purpose:   Dealigns <x> in place by removing characters aligned to
*            gaps (or missing data) in the reference digital aligned
*            sequence <ref_ax>. Gaps and missing data symbols in
*            <ref_ax> are defined by its digital alphabet <abc>.
*
* Returns:   Returns <eslOK> on success; optionally returns the number
*            of characters in the dealigned <x> in <*opt_rlen>.
*
* Throws:    (no abnormal error conditions)
*/
int
esl_abc_XDealign(const ESL_ALPHABET *abc, ESL_DSQ *x, const ESL_DSQ *ref_ax, int64_t *opt_rlen)
{
    int64_t apos;
    int64_t n = 0;

    if (x == NULL) return eslOK;

    x[0] = eslDSQ_SENTINEL;
    for (n=1, apos=1; ref_ax[apos] != eslDSQ_SENTINEL; apos++)
        if (! esl_abc_XIsGap(abc, ref_ax[apos]) && ! esl_abc_XIsMissing(abc, ref_ax[apos]) )
            x[n++] = x[apos];
    x[n] = eslDSQ_SENTINEL;

    if (opt_rlen != NULL) *opt_rlen = n-1;
    return eslOK;
}

/*-------------- end, digital sequences (ESL_DSQ) ---------------*/


/*****************************************************************
* 3. Other routines in the API.
*****************************************************************/ 

/* Function:  esl_abc_GuessAlphabet()
* Synopsis:  Guess alphabet type from residue composition.
* Incept:    SRE, Wed May 16 11:08:29 2007 [Janelia]
*
* Purpose:   Guess the alphabet type from a residue composition.
*            The input <ct[0..25]> array contains observed counts of 
*            the letters A..Z, case-insensitive. 
*            
*            Provided that the compositions contains more than 10
*            residues, the composition is called <eslDNA> if it
*            consists only of the residues ACGTN and all four of ACGT
*            occur (and analogously for <eslRNA>, ACGU$+$N); and it
*            calls the sequence <eslAMINO> either if it contains an
*            amino-specific letter (EFIJLOPQZ), or if it contains at
*            least 15 of the 20 canonical amino acids and consists
*            only of canonical amino acids or X.
*            
*
* Returns:   <eslOK> on success, and <*ret_type> is set to
*            <eslAMINO>, <eslRNA>, or <eslDNA>.
*
*            Returns <eslEAMBIGUOUS> if unable to determine the
*            alphabet type; in this case, <*ret_type> is set to 
*            <eslUNKNOWN>.
*/
int
esl_abc_GuessAlphabet(const int64_t *ct, int *ret_type)
{
    int      type = eslUNKNOWN;
    char     aaonly[]    = "EFIJLOPQZ";
    char     allcanon[]  = "ACG";
    char     aacanon[]   = "DHKMRSVWY";
    int64_t  n1, n2, n3, nn, nt, nu, nx, n; /* n's are counts */
    int      x1, x2, x3, xn, xt, xu;	      /* x's are how many different residues are represented */
    int      i, x;

    x1 = x2 = x3 = xn = xt = xu = 0;
    n1 = n2 = n3 = n = 0;
    for (i = 0; i < 26;                i++) n  += ct[i];
    for (i = 0; aaonly[i]   != '\0'; i++) { x = ct[aaonly[i]   - 'A']; if (x > 0) { n1 += x; x1++; } }
    for (i = 0; allcanon[i] != '\0'; i++) { x = ct[allcanon[i] - 'A']; if (x > 0) { n2 += x; x2++; } }
    for (i = 0; aacanon[i]  != '\0'; i++) { x = ct[aacanon[i]  - 'A']; if (x > 0) { n3 += x; x3++; } }
    nt = ct['T' - 'A']; xt = (nt > 0) ? 1 : 0;
    nu = ct['U' - 'A']; xu = (nu > 0) ? 1 : 0;
    nx = ct['X' - 'A']; 
    nn = ct['N' - 'A']; 

    if      (n  <= 10)                                                type = eslUNKNOWN;
    else if (n1 > 0)                                                  type = eslAMINO; /* contains giveaway, aa-only chars */
    else if (n2+nt+nn == n && x2+xt == 4)                             type = eslDNA;   /* all DNA canon (or N), all four seen */
    else if (n2+nu+nn == n && x2+xu == 4)                             type = eslRNA;   /* all RNA canon (or N), all four seen */
    else if (n1+n2+n3+nn+nt+nx == n && n3>n2 && x1+x2+x3+xn+xt >= 15) type = eslAMINO; /* all aa canon (or X); more aa canon than ambig; all 20 seen */

    *ret_type = type;
    if (type == eslUNKNOWN) return eslEAMBIGUOUS;
    else                    return eslOK;
}



/* Function:  esl_abc_Match()
* Synopsis:  Returns the probability that two symbols match.
* Incept:    SRE, Sun Sep 17 11:46:32 2006 [Janelia]
*
* Purpose:   Given two digital symbols <x> and <y> in alphabet
*            <abc>, calculate and return the probability that
*            <x> and <y> match, taking degenerate residue codes
*            into account.
*            
*            If <p> residue probability vector is NULL, the
*            calculation is a simple average. For example, for DNA,
*            R/A gives 0.5, C/N gives 0.25, N/R gives 0.25, R/R gives
*            0.5.
*            
*            If <p> residue probability vector is non-NULL, it gives
*            a 0..K-1 array of background frequencies, and the
*            returned match probability is an expectation (weighted
*            average) given those residue frequencies.
*            
*            <x> and <y> should only be residue codes. Any other
*            comparison, including comparisons involving gap or
*            missing data characters, or even comparisons involving
*            illegal digital codes, returns 0.0.
*            
*            Note that comparison of residues from "identical"
*            sequences (even a self-comparison) will not result in an
*            identity of 1.0, if the sequence(s) contain degenerate
*            residue codes.
*
* Args:      abc   - digtal alphabet to use
*            x,y   - two symbols to compare
*            p     - NULL, or background probabilities of the
*                    canonical residues in this alphabet [0..K-1]
*
* Returns:   the probability of an identity (match) between
*            residues <x> and <y>.
*/
double
esl_abc_Match(const ESL_ALPHABET *abc, ESL_DSQ x, ESL_DSQ y, double *p)
{
    int    i;
    double prob;
    double sx, sy;

    /* Easy cases */
    if (esl_abc_XIsCanonical(abc, x) && esl_abc_XIsCanonical(abc, y))  
    { 
        if (x==y) return 1.0; else return 0.0;
    }
    if ( ! esl_abc_XIsResidue(abc, x) || ! esl_abc_XIsResidue(abc, x))  return 0.0;

    /* Else, we have at least one degenerate residue, so calc an average or expectation.
    */
    if (p != NULL) 
    {
        prob = sx = sy = 0.;
        for (i = 0; i < abc->K; i++)
        {
            if (abc->degen[(int)x][i])                            sx += p[i];
            if (abc->degen[(int)y][i])                            sy += p[i];
            if (abc->degen[(int)x][i] && abc->degen[(int)x][i]) prob += p[i] * p[i];
        }
        prob = prob / (sx*sy);
    }
    else
    {
        double uniformp = 1. / (double) abc->K;
        prob = sx = sy = 0.;
        for (i = 0; i < abc->K; i++)
        {
            if (abc->degen[(int)x][i])                            sx += uniformp;
            if (abc->degen[(int)y][i])                            sy += uniformp;
            if (abc->degen[(int)x][i] && abc->degen[(int)x][i]) prob += uniformp * uniformp;
        }
        prob = prob / (sx*sy);
    }
    return prob;
}



/* Function:  esl_abc_IAvgScore()
* Synopsis:  Returns average score for degenerate residue.
* Incept:    SRE, Tue Dec 21 10:53:57 2004 [Zaragoza]
*
* Purpose:  Given a residue code <x> in alphabet <a>, and an array of
*           integer scores <sc> for the residues in the base
*           alphabet, calculate and return the average score
*           (rounded to nearest integer).
*           
*           <x> would usually be a degeneracy code, but it
*           may also be a canonical residue. It must not
*           be a gap, missing data, or illegal symbol; if it
*           is, these functions return a score of 0 without
*           raising an error.
*           
*           <esl_abc_FAvgScore()> and <esl_abc_DAvgScore()> do the
*           same, but for float and double scores instead of integers
*           (and for real-valued scores, no rounding is done).
*           
* Args:     a   - digital alphabet to use
*           x   - a symbol to score
*           sc  - score vector for canonical residues [0..K-1]
*           
* Returns:  average score for symbol <x>          
*/
int
esl_abc_IAvgScore(const ESL_ALPHABET *a, ESL_DSQ x, const int *sc)
{
    float result = 0.;
    int i;

    if (! esl_abc_XIsResidue(a, x)) return 0;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) result += (float) sc[i];
    result /= (float) a->ndegen[(int) x];
    if (result < 0) return (int) (result - 0.5);
    else            return (int) (result + 0.5);
}
float
esl_abc_FAvgScore(const ESL_ALPHABET *a, ESL_DSQ x, const float *sc)
{
    float result = 0.;
    int   i;

    if (! esl_abc_XIsResidue(a, x)) return 0.;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) result += sc[i];
    result /= (float) a->ndegen[(int) x];
    return result;
}
double
esl_abc_DAvgScore(const ESL_ALPHABET *a, ESL_DSQ x, const double *sc)
{
    double result = 0.;
    int    i;

    if (! esl_abc_XIsResidue(a, x)) return 0.;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) result += sc[i];
    result /= (double) a->ndegen[(int) x];
    return result;
}


/* Function:  esl_abc_IExpectScore()
* Synopsis:  Returns expected score for degenerate residue.
* Incept:    SRE, Tue Dec 21 11:02:46 2004 [Zaragoza]
*
* Purpose:   Given a residue code <x> in alphabet <a>, an
*            array of integer scores <sc> for the residues in the base
*            alphabet, and background frequencies <p> for the
*            occurrence frequencies of the residues in the base
*            alphabet, calculate and return the expected score
*            (weighted by the occurrence frequencies <p>).
*            
*            <x> would usually be a degeneracy code, but it
*            may also be a canonical residue. It must not
*            be a gap, missing data, or illegal symbol; if it
*            is, these functions return a score of 0 without
*            raising an error.
*
*            <esl_abc_FExpectScore()> and <esl_abc_DExpectScore()> do the
*            same, but for float and double scores instead of integers
*            (for real-valued scores, no rounding is done).
*
* Args:     a   - digital alphabet to use
*           x   - a symbol to score
*           sc  - score vector for canonical residues [0..K-1]
*           p   - background prob's of canonicals     [0..K-1]
*           
* Returns:  average score for symbol <x>          
*/
int
esl_abc_IExpectScore(const ESL_ALPHABET *a, ESL_DSQ x, const int *sc, const float *p)
{
    float  result = 0.;
    float  denom  = 0.;
    int    i;

    if (! esl_abc_XIsResidue(a, x)) return 0;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) { 
            result += (float) sc[i] * p[i];
            denom  += p[i];
        }
        result /= denom;
        if (result < 0) return (int) (result - 0.5);
        else            return (int) (result + 0.5);
}
float
esl_abc_FExpectScore(const ESL_ALPHABET *a, ESL_DSQ x, const float *sc, const float *p)
{
    float  result = 0.;
    float  denom  = 0.;
    int    i;

    if (! esl_abc_XIsResidue(a, x)) return 0.;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) { 
            result += sc[i] * p[i];
            denom  += p[i];
        }
        result /= denom;
        return result;
}
double
esl_abc_DExpectScore(const ESL_ALPHABET *a, ESL_DSQ x, const double *sc, const double *p)
{
    double result = 0.;
    double denom  = 0.;
    int    i;

    if (! esl_abc_XIsResidue(a, x)) return 0.;
    for (i = 0; i < a->K; i++)
        if (a->degen[(int) x][i]) { 
            result += sc[i] * p[i];
            denom  += p[i];
        }
        result /= denom;
        return result;
}

/* Function:  esl_abc_IAvgScVec()
* Synopsis:  Fill out score vector with average degenerate scores.
* Incept:    SRE, Thu Apr  6 12:12:25 2006 [AA890 enroute to Boston]
*
* Purpose:   Given an alphabet <a> and a score vector <sc> of length
*            <a->Kp> that contains integer scores for the base
*            alphabet (<0..a->K-1>), fill out the rest of the score 
*            vector, calculating average scores for 
*            degenerate residues using <esl_abc_IAvgScore()>.
*            
*            The score, if any, for a gap character <K>, the
*            nonresidue <Kp-2>, and the missing data character <Kp-1>
*            are untouched by this function. Only the degenerate
*            scores <K+1..Kp-3> are filled in.
*            
*            <esl_abc_FAvgScVec()> and <esl_abc_DAvgScVec()> do
*            the same, but for score vectors of floats or doubles,
*            respectively.
*
* Returns:   <eslOK> on success.
*/
int
esl_abc_IAvgScVec(const ESL_ALPHABET *a, int *sc)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_IAvgScore(a, x, sc);
    return eslOK;
}
int
esl_abc_FAvgScVec(const ESL_ALPHABET *a, float *sc)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_FAvgScore(a, x, sc);
    return eslOK;
}
int
esl_abc_DAvgScVec(const ESL_ALPHABET *a, double *sc)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_DAvgScore(a, x, sc);
    return eslOK;
}

/* Function:  esl_abc_IExpectScVec()
* Synopsis:  Fill out score vector with average expected scores.
* Incept:    SRE, Thu Apr  6 12:23:52 2006 [AA 890 enroute to Boston]
*
* Purpose:   Given an alphabet <a>, a score vector <sc> of length
*            <a->Kp> that contains integer scores for the base
*            alphabet (<0..a->K-1>), and residue occurrence probabilities
*            <p[0..a->K-1]>; fill in the scores for the
*            degenerate residues <K+1..Kp-3> using <esl_abc_IExpectScore()>.
*            
*            The score, if any, for a gap character <K>, the
*            nonresidue <Kp-2>, and the missing data character <Kp-1>
*            are untouched by this function. Only the degenerate
*            scores <K+1..Kp-3> are filled in.
*            
*            <esl_abc_FExpectScVec()> and <esl_abc_DExpectScVec()> do
*            the same, but for score vectors of floats or doubles,
*            respectively. The probabilities <p> are floats for the
*            integer and float versions, and doubles for the double
*            version.
*
* Returns:   <eslOK> on success.
*/
int
esl_abc_IExpectScVec(const ESL_ALPHABET *a, int *sc, const float *p)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_IExpectScore(a, x, sc, p);
    return eslOK;
}
int
esl_abc_FExpectScVec(const ESL_ALPHABET *a, float *sc, const float *p)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_FExpectScore(a, x, sc, p);
    return eslOK;
}
int
esl_abc_DExpectScVec(const ESL_ALPHABET *a, double *sc, const double *p)
{
    ESL_DSQ x;
    for (x = a->K+1; x <= a->Kp-3; x++)
        sc[x] = esl_abc_DExpectScore(a, x, sc, p);
    return eslOK;
}


/* Function:  esl_abc_FCount()
* Synopsis:  Count a degenerate symbol into a count vector.
* Incept:    SRE, Wed Apr 12 17:16:35 2006 [St. Louis]
*
* Purpose:   Count a possibly degenerate digital symbol <x> (0..Kp-1)
*            into a counts array <ct> for base symbols (0..K-1).
*            Assign the symbol a weight of <wt> (often just 1.0).
*            The count weight of a degenerate symbol is divided equally
*            across the possible base symbols. 
*            
*            <x> can be a gap; if so, <ct> must be allocated 0..K,
*            not 0..K-1. If <x> is a missing data symbol, or a nonresidue
*            data symbol, nothing is done.
*            
*            <esl_abc_DCount()> does the same, but for double-precision
*            count vectors and weights.
*
* Returns:   <eslOK> on success.
*/
int
esl_abc_FCount(const ESL_ALPHABET *abc, float *ct, ESL_DSQ x, float wt)
{
    ESL_DSQ y;

    if (esl_abc_XIsCanonical(abc, x) || esl_abc_XIsGap(abc, x))
        ct[x] += wt;
    else if (esl_abc_XIsMissing(abc, x) || esl_abc_XIsNonresidue(abc, x))
        return eslOK;
    else
        for (y = 0; y < abc->K; y++) {
            if (abc->degen[x][y])
                ct[y] += wt / (float) abc->ndegen[x];
        }
        return eslOK;
}
int
esl_abc_DCount(const ESL_ALPHABET *abc, double *ct, ESL_DSQ x, double wt)
{
    ESL_DSQ y;

    if (esl_abc_XIsCanonical(abc, x) || esl_abc_XIsGap(abc, x))
        ct[x] += wt;
    else if (esl_abc_XIsMissing(abc, x) || esl_abc_XIsNonresidue(abc, x))
        return eslOK;
    else
        for (y = 0; y < abc->K; y++) {
            if (abc->degen[x][y])
                ct[y] += wt / (double) abc->ndegen[x];
        }
        return eslOK;
}

/* Function:  esl_abc_EncodeType()
* Synopsis:  Convert descriptive string to alphabet type code.
* Incept:    SRE, Mon Oct 13 14:52:18 2008 [Janelia]
*
* Purpose:   Convert a string like "amino" or "DNA" to the
*            corresponding Easel internal alphabet type code
*            such as <eslAMINO> or <eslDNA>; return the code.
*
* Returns:   the code, such as <eslAMINO>; if <type> isn't
*            recognized, returns <eslUNKNOWN>.
*/
int
esl_abc_EncodeType(char *type)
{
    if      (esl_strcasecmp(type, "amino") == 0) return eslAMINO;
    else if (esl_strcasecmp(type, "rna")   == 0) return eslRNA;
    else if (esl_strcasecmp(type, "dna")   == 0) return eslDNA;
    else if (esl_strcasecmp(type, "coins") == 0) return eslCOINS;
    else if (esl_strcasecmp(type, "dice")  == 0) return eslDICE;
    else if (esl_strcasecmp(type, "custom")== 0) return eslNONSTANDARD;
    else                                     return eslUNKNOWN;
}

/* Function:  esl_abc_DecodeType()
* Synopsis:  Returns descriptive string for alphabet type code.
* Incept:    SRE, Wed Apr 12 12:23:24 2006 [St. Louis]
*
* Purpose:   For diagnostics and other output: given an internal
*            alphabet code <type> (<eslRNA>, for example), return
*            pointer to an internal string ("RNA", for example). 
*/
char *
esl_abc_DecodeType(int type)
{
    switch (type) {
  case eslUNKNOWN:     return "unknown";
  case eslRNA:         return "RNA";
  case eslDNA:         return "DNA";
  case eslAMINO:       return "amino";
  case eslCOINS:       return "coins";
  case eslDICE:        return "dice";
  case eslNONSTANDARD: return "custom";
  default:             break;
    }
    esl_exception(eslEINVAL, __FILE__, __LINE__, "no such alphabet type code %d\n", type);
    return NULL;
}


/* Function:  esl_abc_ValidateSeq()
* Synopsis:  Assure that a text sequence can be digitized.
* Incept:    SRE, Sat Aug 26 17:40:00 2006 [St. Louis]
*
* Purpose:   Check that sequence <seq> of length <L> can be digitized
*            without error; all its symbols are valid in alphabet
*            <a>. If so, return <eslOK>. If not, return <eslEINVAL>.
*            
*            <errbuf> is either passed as <NULL>, or a pointer to an
*            error string buffer allocated by the caller for
*            <eslERRBUFSIZE> characters. If <errbuf> is non-NULL, and
*            the sequence is invalid, an error message is placed in
*            <errbuf>.
*
* Args:      a      - digital alphabet
*            seq    - sequence to validate [0..L-1]; NUL-termination unnecessary
*            L      - length of <seq>
*            errbuf - NULL, or ptr to <eslERRBUFSIZE> chars of allocated space 
*                     for an error message.
*
* Returns:   <eslOK> if <seq> is valid; <eslEINVAL> if not.
*
* Throws:    (no abnormal error conditions).
*/
int
esl_abc_ValidateSeq(const ESL_ALPHABET *a, const char *seq, int64_t L, char *errbuf)
{
    int     status;
    int64_t i;
    int64_t firstpos = -1;
    int64_t nbad     = 0;

    if (errbuf) *errbuf = 0;
    for (i = 0; i < L; i++) {
        if (! esl_abc_CIsValid(a, seq[i])) {
            if (firstpos == -1) firstpos = i;
            nbad++;
        }
    }
    if (nbad > 0) ESL_XFAIL( eslEINVAL, errbuf, "bad chars found in sequence" );
  return eslOK;

  ERROR:
    return status;
}
/*---------------- end, other API functions ---------------------*/

/*****************************************************************
* Easel - a library of C functions for biological sequence analysis
* Version h3.0; March 2010
* Copyright (C) 2010 Howard Hughes Medical Institute.
* Other copyrights also apply. See the COPYRIGHT file for a full list.
* 
* Easel is distributed under the Janelia Farm Software License, a BSD
* license. See the LICENSE file for more details.
*****************************************************************/
