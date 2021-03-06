/**************************************************
 Created July 2016 by R.Kessler

 utility to read info needed to simulation spectra and/or IFU.
 Used by kcor and snlc_sim programs.

 Strategy:
   User provides table of wavelength bins, and in each bin gives
   SNR1 & SNR2 for the two MAGREF values. This code then solves for 
   effective zeroPoint (Zpe) and skyNoise (includes readout), 
   which can be applied to any MAG.  Each pair of SNR values can 
   be given for several different exposure times,
   allowing interpolation between exposure times. This feature
   allows chaning the exposure time for SN at different redshifts.

  To use this function, kcor input file should contain a key
     SPECTROGRAPH:  <fileName>

  where <fileName> contains

  INSTRUMENT:  <name>
  MAGREF_LIST:  <magref1>  <magref2>
  TEXPOSE_LIST: <t_1>  <t_2> ... <t_n>
 
  SPECBIN: <minL> <maxL>  <sigL> SNR1(t_1) SNR2(t_1) . . SNR1(t_n) SNR2(t_n)
  SPECBIN: <minL> <maxL>  <sigL> SNR1(t_1) SNR2(t_1) . . SNR1(t_n) SNR2(t_n)
  SPECBIN: <minL> <maxL>  <sigL> SNR1(t_1) SNR2(t_1) . . SNR1(t_n) SNR2(t_n)

  etc ...

  Note that minLam,maxLam are specified for each bin so that
  non-uniform bins are allowed.

*********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

#include "sntools.h"
#include "sntools_spectrograph.h"
#include "sntools_fitsio.h"

// =======================================
void init_spectrograph(char *inFile, char *stringOpt ) {

  //  char fnam[] = "init_spectrograph" ;

  // ------------- BEGIN ---------------

  // check option(s) 
  parse_spectrograph_options(stringOpt);

  // read wavelenth bins and list of SNR1,SNR2 from ascii file
  read_spectrograph_text(inFile);

  // solve for ZP and SIGSKY for each SNR[1,2] pair
  solve_spectrograph(); 

  printf("\n"); fflush(stdout);

} // end init_spectrograph

// ===============================================
void parse_spectrograph_options(char *stringOpt) {

#define MXOPT_SPEC 10
  int  opt, NOPT, NTMP ; 
  char *ptrOpt[MXOPT_SPEC], s[MXOPT_SPEC][40];
  char *ptr2[MXOPT_SPEC], s2[2][40];
  char comma[]=",", equal[]="=" ;
  //   char fnam[] = "parse_spectrograph_options" ;

  // ------------- BEGIN --------------

  INPUTS_SPECTRO.NREBIN_LAM=1 ;

  if ( IGNOREFILE(stringOpt) ) { return ; }

  // split by comma-separated values
  for(opt=0; opt < MXOPT_SPEC; opt++ )  { ptrOpt[opt] = s[opt] ; }

  splitString(stringOpt, comma, MXOPT_SPEC,           // inputs               
              &NOPT, ptrOpt );                        // outputs             

  // split each option by equal sign:  bla=val
  ptr2[0] = s2[0];
  ptr2[1] = s2[1];
  for(opt=0; opt < NOPT; opt++ ) {
    splitString(s[opt], equal, MXOPT_SPEC, &NTMP, ptr2);

    if ( strcmp(s2[0],"rebin")==0 ) {
      sscanf(s2[1] , "%d", &INPUTS_SPECTRO.NREBIN_LAM ); 
      printf("\t Spectrograph option: rebin wavelength by %d\n",
	     INPUTS_SPECTRO.NREBIN_LAM) ;
    }
  }

  return ;

} // end parse_spectrograph_options

// ===============================================
void read_spectrograph_text(char *inFile) {

  FILE *fp ;
  int NROW_FILE, ikey, NKEY_FOUND, DONE_MALLOC ;
  int NBL, NBT, NRDCOL, t ;
  char c_get[60], tmpLine[200] ;
  char fnam[] = "read_spectrograph_text" ;

#define NKEY_REQ_SPECTROGRAPH 3
#define IKEY_INSTRUMENT 0
#define IKEY_MAGREF     1
#define IKEY_TEXPOSE    2

  const char KEYREQ_LIST[NKEY_REQ_SPECTROGRAPH][40] =  { 
    "INSTRUMENT:", 
    "MAGREF_LIST:", 
    "TEXPOSE_LIST:"  
  } ;
  int KEYFLAG_FOUND[NKEY_REQ_SPECTROGRAPH];

  // ---------------- BEGIN -------------

  printf("\n %s: \n", fnam); fflush(stdout);

  // get number of rows in file to get malloc size.
  NROW_FILE = nrow_read(inFile,fnam);

  fp = fopen(inFile,"rt");
  if ( !fp ) {
    sprintf(c1err,"Could not find SPECTROGRAPH table file:");
    sprintf(c2err,"%s", inFile );
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
  }

  sprintf(INPUTS_SPECTRO.INFILE_NAME,      "inFile"  );
  sprintf(INPUTS_SPECTRO.INSTRUMENT_NAME,  "UNKNOWN" );
  INPUTS_SPECTRO.MAGREF_LIST[0] = 99. ;
  INPUTS_SPECTRO.MAGREF_LIST[1] = 99. ;
  INPUTS_SPECTRO.NBIN_TEXPOSE = 0 ;
  INPUTS_SPECTRO.NBIN_LAM     = 0 ;
  INPUTS_SPECTRO.NBIN_LAM_noREBIN = 0 ;  
  INPUTS_SPECTRO.LAM_MIN     = INPUTS_SPECTRO.LAM_MAX     = -9.0 ;
  INPUTS_SPECTRO.TEXPOSE_MIN = INPUTS_SPECTRO.TEXPOSE_MAX = -9.0 ;

  NERR_SNR_SPECTROGRAPH = 0 ;

  NKEY_FOUND = DONE_MALLOC = 0 ;
  for(ikey=0; ikey < NKEY_REQ_SPECTROGRAPH; ikey++ ) 
    { KEYFLAG_FOUND[ikey] = 0; }

  NBL = NBT = 0 ;

  printf("    Open %s \n", inFile ); fflush(stdout);

  reset_VALUES_SPECBIN(); 

  while( (fscanf(fp, "%s", c_get)) != EOF) {


    // if comment key is found, read remainder of line into dummy string  
    // so that anything after comment key is ignored (even a valid key)  
    if ( c_get[0] == '#' || c_get[0] == '!' || c_get[0] == '%' )
      { fgets(tmpLine, 80, fp) ; continue ; }

    // check all default keys
    for(ikey=0; ikey < NKEY_REQ_SPECTROGRAPH; ikey++ ) {
      if ( strcmp(c_get, KEYREQ_LIST[ikey] ) == 0 ) { 
	NKEY_FOUND++ ;  KEYFLAG_FOUND[ikey] = 1;

	switch(ikey) {
	case IKEY_INSTRUMENT :
	  readchar(fp, INPUTS_SPECTRO.INSTRUMENT_NAME) ; break ;
	case IKEY_MAGREF :
	  readdouble(fp, 2, INPUTS_SPECTRO.MAGREF_LIST ) ; break ;
	case IKEY_TEXPOSE :
	  NBT = read_TEXPOSE_LIST(fp); break ;
	default:
	  sprintf(c1err,"Unknow KEY[%d] = %s", ikey, KEYREQ_LIST[ikey] );
	  sprintf(c2err,"Check code and file=%s", inFile );
	  errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
	  break ;
	}

      } // end string match
    } // end ikey

    // - - - - - - - - - - - - - - - - - - 
    if( NKEY_FOUND == NKEY_REQ_SPECTROGRAPH && DONE_MALLOC==0 ) {
      // malloc array vs. lambda and TEXPOSE
      malloc_spectrograph(+1, NROW_FILE, NBT);

      NRDCOL = 2 + 2*NBT ; // number of columns to read below
      DONE_MALLOC = 1;
    }

    // - - - - - - - - - -

    if ( strcmp(c_get,"SPECBIN:") == 0 ) {

      if ( NKEY_FOUND < NKEY_REQ_SPECTROGRAPH ) { 
	printf("\n PRE-ABORT DUMP: \n");
	for(ikey=0; ikey < NKEY_REQ_SPECTROGRAPH; ikey++ ) {
	  printf("   Required header key:  '%s'   (FOUND=%d) \n", 
		 KEYREQ_LIST[ikey], KEYFLAG_FOUND[ikey] );
	}
	sprintf(c1err,"Found SPECBIN key before required header keys.");
	sprintf(c2err,"Read %d of %d required keys.",
		NKEY_FOUND, NKEY_REQ_SPECTROGRAPH );
	errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
      } // end ABORT

      NBL = read_SPECBIN_spectrograph(fp);

    } // end SPECBIN line

  }  // end while
  

  if ( NERR_SNR_SPECTROGRAPH > 0 ) {
    sprintf(c1err,"Found %d errors for which", NERR_SNR_SPECTROGRAPH);
    sprintf(c2err,"SNR(Texpose) is NOT monotically increasing");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  // ---------------------------------------

  printf("    Read %d LAMBDA bins from %.0f to %.0f A \n",
	 NBL, INPUTS_SPECTRO.LAM_MIN, INPUTS_SPECTRO.LAM_MAX ); 

  printf("    Read %d TEXPOSE values: ", NBT ); 
  for(t=0; t < NBT; t++ ) 
    { printf("%d ", (int)INPUTS_SPECTRO.TEXPOSE_LIST[t]); }
  printf(" sec \n");

  // -----
  if ( NBL >= MXLAM_SPECTROGRAPH ) {
    sprintf(c1err,"NBIN_LAM=%d exceeds MXLAM_SPECTROGRAPH=%d",
	    NBL, MXLAM_SPECTROGRAPH );
    sprintf(c2err,"Check spectrograph file");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
  }

  fflush(stdout);
  fclose(fp);

  return ;

} // end read_spectrograph_text

// ========================================================
int read_SPECBIN_spectrograph(FILE *fp) {

  // read values after SPECBIN key

  int NBT    = INPUTS_SPECTRO.NBIN_TEXPOSE ;
  int NBL    = INPUTS_SPECTRO.NBIN_LAM ;
  int NRDCOL = NCOL_noSNR + 2*NBT ;  // LAMMIN,LAMMAX,LAMRES + SNR values
  int NREBIN_LAM = INPUTS_SPECTRO.NREBIN_LAM ;
  int jtmp, t, NBMOD ;
  double XTMP[MXVALUES_SPECBIN] ;
  double SNR_OLD, SNR_NEW ;
  char fnam[] = "read_SPECBIN_spectrograph" ;

  // ------------ BEGIN -------------

  readdouble(fp, NRDCOL, XTMP );
  if ( NREBIN_LAM == 1 ) { goto PARSE_XTMP ; }

  // ----------------------
  // check rebinning
  INPUTS_SPECTRO.NBIN_LAM_noREBIN++ ;
  NBMOD = INPUTS_SPECTRO.NBIN_LAM_noREBIN%NREBIN_LAM ;
  if ( NBMOD == 1 )  { VALUES_SPECBIN[0] = XTMP[0] ; }
  VALUES_SPECBIN[1] = XTMP[1] ;

  // always increment SNR in quadrature
  for(jtmp=NCOL_noSNR; jtmp < NRDCOL; jtmp++ ) {
    SNR_OLD = VALUES_SPECBIN[jtmp] ;
    SNR_NEW = XTMP[jtmp];
    VALUES_SPECBIN[jtmp] = sqrt(SNR_OLD*SNR_OLD + SNR_NEW*SNR_NEW);
  }
  if ( NBMOD != 0 ) { return(NBL); }

  // -----------------------------------------
  // transfer global VALUES_SPECBIN back to local XTMP
  for(jtmp=0; jtmp < NRDCOL; jtmp++ )
    { XTMP[jtmp] = VALUES_SPECBIN[jtmp] ; }

 PARSE_XTMP:

  if ( XTMP[1] < XTMP[0] ) {
    sprintf(c1err,"LAMMAX=%f < LAMMIN=%f ???", XTMP[1], XTMP[0] );
    sprintf(c2err,"Check LAMBDA binning in SPECTROGRAPH file.");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
  }
 

  INPUTS_SPECTRO.LAMMIN_LIST[NBL] = XTMP[0] ;
  INPUTS_SPECTRO.LAMMAX_LIST[NBL] = XTMP[1] ;
  INPUTS_SPECTRO.LAMSIGMA_LIST[NBL] = XTMP[2] ;
  INPUTS_SPECTRO.LAMAVG_LIST[NBL] = ( XTMP[0] + XTMP[1] ) / 2.0 ;
  INPUTS_SPECTRO.LAMBIN_LIST[NBL] = ( XTMP[1] - XTMP[0] ) ;

  jtmp=NCOL_noSNR ;
  for(t=0; t < NBT; t++ ) {
    INPUTS_SPECTRO.SNR0[NBL][t]  = XTMP[jtmp] ;  jtmp++ ;
    INPUTS_SPECTRO.SNR1[NBL][t]  = XTMP[jtmp] ;  jtmp++ ;
    NERR_SNR_SPECTROGRAPH += check_SNR_SPECTROGRAPH(NBL,t);
  }
  NBL++ ;  
  INPUTS_SPECTRO.NBIN_LAM = NBL ;


  // keep track of global min/max
  if ( NBL == 1 ) { INPUTS_SPECTRO.LAM_MIN = XTMP[0]; }
  INPUTS_SPECTRO.LAM_MAX = XTMP[1];
  
  reset_VALUES_SPECBIN(); 

  return(NBL) ;

} // end read_SPECBIN_spectrograph


// ========================================================
int check_SNR_SPECTROGRAPH(int l, int t) {

  // make sure that SNR(t) is increasing, where
  // t = TEXPOSURE index
  // l = lambda index
  //
  // Returns 0 for success; returns 1 on error so that
  // calling function can count errors.

  char fnam[] = "check_SNR_SPECTROGRAPH" ;

  // ---------------BEGIN ----------

  if ( t == 0 ) { return(0) ;}

  if ( INPUTS_SPECTRO.SNR0[l][t] < INPUTS_SPECTRO.SNR0[l][t-1] ) {
    printf("\n# - - - - - - - - - - - - - - - - - - - - - - - - -\n");
    printf(" PRE-WARNING DUMP: \n" ) ;
    printf("\t LAMBDA(l=%d) = %f \n", l, INPUTS_SPECTRO.LAMAVG_LIST[l] );
    printf("\t SNR0(Texpose=%.2f) = %f (t=%d)\n", 
	   INPUTS_SPECTRO.TEXPOSE_LIST[t-1], INPUTS_SPECTRO.SNR0[l][t-1],t-1);
    printf("\t SNR0(Texpose=%.2f) = %f (t=%d)\n", 
	   INPUTS_SPECTRO.TEXPOSE_LIST[t],  INPUTS_SPECTRO.SNR0[l][t], t );
    sprintf(c1err,"SNR0 is not monotonic");
    sprintf(c2err,"Check SPECTROGRAPH table");
    errmsg(SEV_WARN, 0, fnam, c1err, c2err); 
    return(1);
  }

  if ( INPUTS_SPECTRO.SNR1[l][t] < INPUTS_SPECTRO.SNR1[l][t-1] ) {
    printf("\n# - - - - - - - - - - - - - - - - - - - - - - - - -\n");
    printf(" PRE-WARNING DUMP: \n" ) ;
    printf("\t LAMBDA(l=%d) = %f \n", l, INPUTS_SPECTRO.LAMAVG_LIST[l] );
    printf("\t SNR1(Texpose=%.2f) = %f (t=%d) \n", 
	   INPUTS_SPECTRO.TEXPOSE_LIST[t-1], INPUTS_SPECTRO.SNR1[l][t-1],t-1 );
    printf("\t SNR1(Texpose=%.2f) = %f (t=%d) \n", 
	   INPUTS_SPECTRO.TEXPOSE_LIST[t], INPUTS_SPECTRO.SNR1[l][t], t );
    sprintf(c1err,"SNR1 is not monotonic");
    sprintf(c2err,"Check SPECTROGRAPH table");
    errmsg(SEV_WARN, 0, fnam, c1err, c2err); 
    return(1);
  }

  return(0)
;
} // end check_SNR_SPECTROGRAPH

// ========================================================
void reset_VALUES_SPECBIN(void) {

  int jtmp;
  for(jtmp=0; jtmp < MXVALUES_SPECBIN; jtmp++) 
    { VALUES_SPECBIN[jtmp] = 0.0; } 

} // end reset_VALUES_SPECBIN

// ========================================================
int read_TEXPOSE_LIST(FILE *fp) {

  // read all exposure times on this line; 
  // stop reading at end-of-line or at comment string

#define MXCHAR_TEXPOSE_LIST 400  // max chars to read on line
  int NBT=0 ;
  double T, TLAST;
  char tmpLine[MXCHAR_TEXPOSE_LIST], *ptrtok, cval[40] ;
  char fnam[] = "read_TEXPOSE_LIST";

  // ---------- BEGIN --------------

  fgets(tmpLine, MXCHAR_TEXPOSE_LIST, fp) ; 
  ptrtok = strtok(tmpLine," ") ; // split string   

  while ( ptrtok != NULL  ) {
    sprintf(cval, "%s", ptrtok );
    
    //    printf(" xxx cval[NBT=%d] = '%s' \n", NBT, cval );

    if ( cval[0] == '#'  ) { goto DONE ; }
    if ( cval[0] == '%'  ) { goto DONE ; }
    if ( cval[0] == '!'  ) { goto DONE ; }
    if ( cval[0] == '\r' ) { goto DONE ; } // <CR>

    if ( NBT < MXTEXPOSE_SPECTROGRAPH ) 
      { sscanf(cval , "%le", &INPUTS_SPECTRO.TEXPOSE_LIST[NBT] ); }

    // make sure each TEXPOSE is increasing 
    if ( NBT>0 ) {
      T     = INPUTS_SPECTRO.TEXPOSE_LIST[NBT] ;
      TLAST = INPUTS_SPECTRO.TEXPOSE_LIST[NBT-1] ;
      if ( T < TLAST ) {
	sprintf(c1err,"TEXPOSE_LIST must be in increasing order.");
	sprintf(c2err,"TEXPOSE_LIST[%d,%d] = %.2f , %.2f ",
		NBT-1,NBT, TLAST,T);
	errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
    }

    NBT++ ;     
    ptrtok = strtok(NULL, " ");
  }

 DONE:
  INPUTS_SPECTRO.NBIN_TEXPOSE = NBT ;
  INPUTS_SPECTRO.TEXPOSE_MIN  = INPUTS_SPECTRO.TEXPOSE_LIST[0] ;
  INPUTS_SPECTRO.TEXPOSE_MAX  = INPUTS_SPECTRO.TEXPOSE_LIST[NBT-1] ;

  if ( NBT >= MXTEXPOSE_SPECTROGRAPH ) {
    sprintf(c1err,"Found %d TEXPOSE_LIST values", NBT);
    sprintf(c2err,"but MXTEXPOSE_SPECTROGRAPH=%d", MXTEXPOSE_SPECTROGRAPH);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
  }

  return(NBT);

} // end read_TEXPOSE_LIST



// ========================================================
void malloc_spectrograph(int OPT, int NBIN_LAM, int NBIN_TEXPOSE) {

  // OPT = +1 --> malloc
  // OPT = -1 --> free
  
  int  MEML0 = NBIN_LAM     * sizeof(double);
  int  MEML1 = NBIN_LAM     * sizeof(double*);
  int  MEMT  = NBIN_TEXPOSE * sizeof(double);
  int  ilam;
  //  char fnam[] = "malloc_spectrograph" ;

  // ------------ BEGIN ---------------

  if ( OPT > 0 ) {
    INPUTS_SPECTRO.LAMMIN_LIST   = (double*) malloc(MEML0);
    INPUTS_SPECTRO.LAMMAX_LIST   = (double*) malloc(MEML0);
    INPUTS_SPECTRO.LAMAVG_LIST   = (double*) malloc(MEML0);
    INPUTS_SPECTRO.LAMSIGMA_LIST = (double*) malloc(MEML0);
    INPUTS_SPECTRO.LAMBIN_LIST   = (double*) malloc(MEML0);

    INPUTS_SPECTRO.SNR0      = (double**) malloc(MEML1);
    INPUTS_SPECTRO.SNR1      = (double**) malloc(MEML1);
    INPUTS_SPECTRO.ZP        = (double**) malloc(MEML1);
    INPUTS_SPECTRO.SQSIGSKY  = (double**) malloc(MEML1);

    for(ilam=0; ilam < NBIN_LAM; ilam++ ) {
      INPUTS_SPECTRO.SNR0[ilam]      = (double*) malloc(MEMT);
      INPUTS_SPECTRO.SNR1[ilam]      = (double*) malloc(MEMT);
      INPUTS_SPECTRO.ZP[ilam]        = (double*) malloc(MEMT);
      INPUTS_SPECTRO.SQSIGSKY[ilam]  = (double*) malloc(MEMT);

      INPUTS_SPECTRO.LAMMIN_LIST[ilam]   = -999.0 ;
      INPUTS_SPECTRO.LAMMAX_LIST[ilam]   = -99.0 ;
      INPUTS_SPECTRO.LAMAVG_LIST[ilam]   = -9.0 ;
      INPUTS_SPECTRO.LAMSIGMA_LIST[ilam] =  0.0 ;
      INPUTS_SPECTRO.LAMBIN_LIST[ilam]   =  0.0 ;
    }
  }
  else {
    free(INPUTS_SPECTRO.LAMMIN_LIST);
    free(INPUTS_SPECTRO.LAMMAX_LIST);
    free(INPUTS_SPECTRO.LAMAVG_LIST);
    free(INPUTS_SPECTRO.LAMSIGMA_LIST);
    free(INPUTS_SPECTRO.LAMBIN_LIST);
    free(INPUTS_SPECTRO.SNR0);
    free(INPUTS_SPECTRO.SNR1);
    free(INPUTS_SPECTRO.ZP);
    free(INPUTS_SPECTRO.SQSIGSKY);
    for(ilam=0; ilam < NBIN_LAM; ilam++ ) {
      free( INPUTS_SPECTRO.SNR0[ilam]  );
      free( INPUTS_SPECTRO.SNR1[ilam]  );
      free( INPUTS_SPECTRO.ZP[ilam] );
      free( INPUTS_SPECTRO.SQSIGSKY[ilam] );
    }
  }


  return ;

} // end malloc_spectrograph


// =================================
void  solve_spectrograph(void) {

  // in each lambda bin, solve for Zpe and SQSIGSKY
  //

  int l,t, iref ;
  int NBL = INPUTS_SPECTRO.NBIN_LAM ;
  int NBT = INPUTS_SPECTRO.NBIN_TEXPOSE ;

  double MAGREF[2], POWMAG[2], SQPOWMAG[2], ARG, SNR[2] ;
  double TOP, BOT, ZP, SQSIGSKY, F[2], DUM0, DUM1, LAMMIN, LAMMAX, LAMAVG ;
  double SNR_check[2], check[2] ;

  char fnam[] = "solve_spectrograph" ;

  // ------------- BEGIN ---------------

  for(iref=0; iref < 2; iref++ ) {
    MAGREF[iref] = INPUTS_SPECTRO.MAGREF_LIST[iref];
    ARG  = -0.4 * MAGREF[iref] ; 
    POWMAG[iref] = pow(TEN,ARG); 
    SQPOWMAG[iref] = POWMAG[iref] * POWMAG[iref] ;
  }


  for(l=0; l < NBL; l++ ) {

    LAMMIN = INPUTS_SPECTRO.LAMMIN_LIST[l] ;
    LAMMAX = INPUTS_SPECTRO.LAMMAX_LIST[l] ;
    LAMAVG = 0.5 * ( LAMMIN + LAMMAX );

    for(t=0; t < NBT; t++ ) {
      SNR[0] = INPUTS_SPECTRO.SNR0[l][t] ;
      SNR[1] = INPUTS_SPECTRO.SNR1[l][t] ;
      TOP    = POWMAG[0] - POWMAG[1] ;

      DUM0 = POWMAG[0]/SNR[0];
      DUM1 = POWMAG[1]/SNR[1];
      BOT  = DUM0*DUM0 - DUM1*DUM1 ;

      if ( TOP <= 0.0 || BOT <= 0.0 ) {
	printf("\n PRE-ABORT DUMP: \n");
	printf("\t BOT=%le and TOP=%le\n", BOT, TOP);
	printf("\t SNR[0]=%le  SNR[1]=%le \n", SNR[0], SNR[1] );
	sprintf(c1err,"Cannot solve ZP for LAM=%.1f to %.1f,  and t=%d sec",
		LAMMIN, LAMMAX, (int)INPUTS_SPECTRO.TEXPOSE_LIST[t] );
        sprintf(c2err,"Check SPECTROGRAPH") ;
        errmsg(SEV_FATAL, 0, fnam, c1err, c2err);
      }

      ZP   = 2.5*log10(TOP/BOT) ;  // photo-electrons
      F[0]   = pow(TEN, -0.4*(MAGREF[0]-ZP) );
      F[1]   = pow(TEN, -0.4*(MAGREF[1]-ZP) );
      SQSIGSKY = (F[0]/SNR[0])*(F[0]/SNR[0]) - F[0] ;

      // store in global
      INPUTS_SPECTRO.ZP[l][t]        = ZP ;
      INPUTS_SPECTRO.SQSIGSKY[l][t]  = SQSIGSKY ;

      // make sure original SNR can be computed from ZP and SQSIGSKY
      for(iref=0; iref<2; iref++ ) {  
	SNR_check[iref] = F[iref] / sqrt( SQSIGSKY + F[iref] ) ;
	check[iref] = fabs(SNR[iref]/SNR_check[iref]-1.0) ;
      }

      if ( check[0] > 0.001  ||  check[1] > 0.001 ) {
	printf("\n PRE-ABORT DUMP: \n");
	printf("   SNR0(input/check) = %f/%f = %f \n",
	       SNR[0], SNR_check[0], SNR[0]/SNR_check[0] );
	printf("   SNR1(input/check) = %f/%f = %f \n",
	       SNR[1], SNR_check[1], SNR[1]/SNR_check[1] );
	printf("   Lambda bin: %f to %f \n", LAMMIN, LAMMAX);
	printf("   F0=%le   F1=%le  \n", F[0], F[1] );
	sprintf(c1err,"Problem computing ZP and SQSIGSKY" );
	sprintf(c2err,"ZP=%f  SQSKYSIG=%f", ZP, SQSIGSKY );
	errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
      }

    }  // end t bins
  }  // end l bins

  return ;

} // end solve_spectrograph


// ================================================
void get_FILTERtrans_spectrograph(double *LAMFILT_MIN, double *LAMFILT_MAX, 
				  int MXTRANS, int *NBIN_LAMFILT,
				  double *LAMFILT_ARRAY, 
				  double *TRANSFILT_ARRAY ) 
{  
  // Return IFU transmission efficiency (TRANS_ARRAY) vs. 
  // wavelength (LAM_ARRAY), in the wavelength region LMIN to LMAX.
  // If number of wavelength bins (NBIN_TRANS) exceeds MXTRANS --> ABORT.
  // 
  // In each wavelength bin, Eff ~ Flux ~ 10^[-0.4*(MAGREF-ZP)
  //
  // and normlize max trans = 1.0.
  //
  // If there are multuple T_EXPOSE, then use first one since
  // relative trans should be the same regardless of T_EXPOSE.
  //
  // Inputs:
  //  LAMFILT_MIN[MAX] : min,max wavelength of filter slice
  //  MXTRANS    : max size of output array; abort on overflow
  //
  // Outputs:
  //   LAMFILT_MIN[LMAX]  : extended lambda range so that Trans->0 at edges.
  //   NBIN_TRANS  : number of filter-transmission bins
  //   LAM_ARRAY   : wavelength array
  //   TRANS_ARRAY : transmission array
  //
  // Oct 27 2016: protect interpolation in 1st half of 1st bin
  //              and last half of last bin
  //
  // Feb 01 2017: 
  //  + fix malloc and bugs related to mixing up NBL_TRANS [NB]
  //    and NBL_SPECTRO.
  //    
  int l, t, MEMD ;
  double *FLUX_TMP, *ZP_TMP, LCEN, F, ZP, ARG, FLUX_MAX ;
  double LAMRANGE, LAMSTEP, LAMSTEP_APPROX, xlam ;
  int NBL_SPECTRO = INPUTS_SPECTRO.NBIN_LAM ;
  int NBL_TRANS ;
  int OPT_INTERP=1;

  char fnam[] = "get_FILTERtrans_spectrograph" ;

  // -------------- BEGIN -------------

  // define finer lambda bins for intergration, with a minimum of 10 bins.
  LAMSTEP_APPROX = 5.0 ;
  LAMRANGE       = *LAMFILT_MAX - *LAMFILT_MIN ;
  NBL_TRANS      = (int)(LAMRANGE/LAMSTEP_APPROX);   
  if(NBL_TRANS<10) {NBL_TRANS=10;}
  LAMSTEP        = LAMRANGE/(double)NBL_TRANS;

  // add one bin on the low and high side where transmission=0
  *LAMFILT_MIN -= LAMSTEP ;  NBL_TRANS++ ;
  *LAMFILT_MAX += LAMSTEP ;  NBL_TRANS++ ;

  if ( NBL_TRANS >= MXTRANS ) {
    sprintf(c1err,"NBL_TRANS=%d exceeds bound MXTRANS=%d", 
	    NBL_TRANS, MXTRANS);
    sprintf(c2err,"filter lambda range %.1f to %.1f, lamstep=%.3f", 
	    *LAMFILT_MIN, *LAMFILT_MAX, LAMSTEP ) ;
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }


  // allocate local memory
  MEMD     = sizeof(double) ;
  FLUX_TMP = (double*) malloc ( MEMD * NBL_TRANS   ) ;
  ZP_TMP   = (double*) malloc ( MEMD * NBL_SPECTRO ) ;
  FLUX_MAX = 0.0 ;

  // load local ZP_TMP[ilam] array to use for interpolation
  t=0; // always use first T_EXPOSE bin
  for(l=0; l < NBL_SPECTRO ; l++ ) { ZP_TMP[l] = INPUTS_SPECTRO.ZP[l][t];  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - 
  for(l=0; l < NBL_TRANS; l++ ) {

    FLUX_TMP[l] = 0.0 ;

    xlam = (double)l ;
    LCEN = *LAMFILT_MIN + LAMSTEP*(xlam+0.5); // center of bin
    F    = 0.0 ;

    // interpolate ZP
    if ( l==0 || l == NBL_TRANS-1 ) { 
      F  = 0.0 ; // first & last bin has FLUX_TRANS=0
      ZP = 0.0 ;
    }
    else if ( LCEN <= INPUTS_SPECTRO.LAMAVG_LIST[0] ) {
      ZP = ZP_TMP[0];
    }
    else if ( LCEN >= INPUTS_SPECTRO.LAMAVG_LIST[NBL_SPECTRO-1] ) {
      ZP = ZP_TMP[NBL_SPECTRO-1];
    }
    else {
      ZP = interp_1DFUN (OPT_INTERP, LCEN, NBL_SPECTRO, 
			 INPUTS_SPECTRO.LAMAVG_LIST, ZP_TMP, fnam );      
    }

    if ( ZP > 0.001 ) {
      ARG = -0.4*(INPUTS_SPECTRO.MAGREF_LIST[0] - ZP);
      F   = pow(TEN,ARG) ;
      if ( F > FLUX_MAX ) { FLUX_MAX = F ; }
    }

    FLUX_TMP[l]      = F ; 
    LAMFILT_ARRAY[l] = LCEN ;

  } // end lambda loop
  
  // -------------------- xyz
  if ( FLUX_MAX < 1.0E-9 ) {
    sprintf(c1err,"Synthetic FLUX_MAX=%f ", FLUX_MAX);	   
    sprintf(c2err,"LAMFILT_MIN/MAX=%.1f/%.2f  NBL_TRANS=%d", 
	    *LAMFILT_MIN, *LAMFILT_MAX, NBL_TRANS) ;
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  *NBIN_LAMFILT = NBL_TRANS ;
  for(l=0; l < NBL_TRANS ; l++ ) 
    { TRANSFILT_ARRAY[l] =  FLUX_TMP[l] / FLUX_MAX ; }


  free(FLUX_TMP);  free(ZP_TMP);
  return ;

} // end get_FILTERtrans_spectrograph


// =====================================
void read_spectrograph_fits(char *inFile) {

  // Read spectrograph info from FITS-formatted *inFile
  // that was created by kcor.exe.
  // The read-back includes each wavelength bin range,
  // and its ZP and SQSIGSKY. The original SNR1,SNR2 
  // are not stored.
  //
  // if SPECTROGRAPH key is in header, read corresponding table.
  // if SPECTROGRAPH key is not in header, return with no message.
  //
  // Oct 14 2016: read LAMSIGMA_LIST
  // Sep 19 2018: fill INPUTS_SPECTRO.ISFIX_LAMBIN (used for output FORMAT)

  int istat, hdutype, extver, icol, anynul ;
  fitsfile *fp ;

  float  tmpVal_f  ;
  int    NBL, NBT, l, t ;
  double L0, L1  ;

  char keyName[40], comment[80], TBLname[40], INFILE[MXPATHLEN] ;
  //  char fnam[] = "read_spectrograph_fits" ;

  // --------------- BEGIN -----------------

  SPECTROGRAPH_USEFLAG = 0;

  // open fits file
  istat = 0 ;
  sprintf(INFILE, "%s", inFile);
  fits_open_file(&fp, INFILE, READONLY, &istat );
  
  // if inFile does not exist, try official area
  if ( istat != 0 ) {
    sprintf(INFILE, "%s/kcor/%s", PATH_SNDATA_ROOT, inFile);
    istat = 0 ;
    fits_open_file(&fp, INFILE, READONLY, &istat );
  }

  sprintf(c1err,"Open %s", INFILE );
  snfitsio_errorCheck(c1err, istat);

  // -------------------------------------------------
  sprintf(TBLname, "%s", FITSTABLE_NAME_SPECTROGRAPH );

  // -------------------------------------------------
  // check for SPECTROGRAPH keys in header.
  sprintf(comment,"Read spectrograph from kcor file");

  sprintf(keyName, "%s", "SPECTROGRAPH_INSTRUMENT" );
  fits_read_key(fp, TSTRING, keyName, &INPUTS_SPECTRO.INSTRUMENT_NAME, 
		comment, &istat );
  if( istat != 0 ) { goto FCLOSE ; }


  sprintf(keyName, "%s", "SPECTROGRAPH_FILTERLIST" );
  fits_read_key(fp, TSTRING, keyName, &INPUTS_SPECTRO.SYN_FILTERLIST_BAND, 
		comment, &istat );

  printf("\n Read spectrograph instrument '%s' \n", 
	 INPUTS_SPECTRO.INSTRUMENT_NAME );
  fflush(stdout);

  SPECTROGRAPH_USEFLAG = 1 ; // set global flag that spectrograph is defined.

  extver = istat = 0 ;  hdutype = BINARY_TBL ;
  fits_movnam_hdu( fp, hdutype, TBLname, extver, &istat);
  sprintf(c1err,"movnam to %s table (hdutype=%d)",  TBLname, hdutype ) ;
  snfitsio_errorCheck(c1err, istat);

  // - - - - - -
  // read binning
  sprintf(keyName, "%s", "NBL" );
  fits_read_key(fp, TINT, keyName, &NBL, comment, &istat );
  sprintf(c1err,"read number of lambda bins");
  snfitsio_errorCheck(c1err, istat);
  printf("   Found %d wavelength bins \n", NBL);
  
  sprintf(keyName, "%s", "NBT" );
  fits_read_key(fp, TINT, keyName, &NBT, comment, &istat );
  sprintf(c1err,"read number of TEXPOSE bins");
  snfitsio_errorCheck(c1err, istat);
  printf("   Found %d TEXPOSE bins \n", NBT );

  fflush(stdout);
  INPUTS_SPECTRO.NBIN_LAM     = NBL ;
  INPUTS_SPECTRO.NBIN_TEXPOSE = NBT ;
  
  // now read each exposure time:
  printf("\t TEXPOSE(seconds) = ");
  for(t=0; t < NBT; t++ ) {
    sprintf(keyName, "TEXPOSE%2.2d", t );
    fits_read_key(fp, TFLOAT, keyName, &tmpVal_f, comment, &istat );
    sprintf(c1err,"read %s", keyName);
    snfitsio_errorCheck(c1err, istat);

    printf("%d ", (int)tmpVal_f );
    INPUTS_SPECTRO.TEXPOSE_LIST[t] = tmpVal_f ;
  }
  printf("\n"); fflush(stdout);

  // set global min & max
  INPUTS_SPECTRO.TEXPOSE_MIN = INPUTS_SPECTRO.TEXPOSE_LIST[0] ;
  INPUTS_SPECTRO.TEXPOSE_MAX = INPUTS_SPECTRO.TEXPOSE_LIST[NBT-1] ;

  // malloc arrays before reading
  malloc_spectrograph(1,NBL,NBT);

  // ---------------------------
  long NROW, FIRSTELEM, FIRSTROW ;
  FIRSTELEM = FIRSTROW = 1 ;  NROW=NBL ;  anynul=istat=0 ;
  
  // read lambda range for each wavelength bin
  icol = 1 ;
  fits_read_col_dbl(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1D, 
		    INPUTS_SPECTRO.LAMMIN_LIST, &anynul, &istat );
  sprintf(c1err,"read LAMMIN_LIST column" );
  snfitsio_errorCheck(c1err, istat);

  icol = 2 ;
  fits_read_col_dbl(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1D, 
		    INPUTS_SPECTRO.LAMMAX_LIST, &anynul, &istat );
  sprintf(c1err,"read LAMMAX_LIST column" );
  snfitsio_errorCheck(c1err, istat);

  icol = 3 ;
  fits_read_col_dbl(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1D, 
		    INPUTS_SPECTRO.LAMSIGMA_LIST, &anynul, &istat );
  sprintf(c1err,"read LAMSIGMA_LIST column" );
  snfitsio_errorCheck(c1err, istat);

  // compute LAMAVG & LAMBIN
  double LBIN, LASTBIN=0.0 ;
  INPUTS_SPECTRO.FORMAT_MASK = 1; // default: write LAMCEN
  for(l=0; l <NBL; l++ ) {
    L0 = INPUTS_SPECTRO.LAMMIN_LIST[l] ;
    L1 = INPUTS_SPECTRO.LAMMAX_LIST[l] ;
    LBIN = L1-L0 ;
    INPUTS_SPECTRO.LAMAVG_LIST[l] = ( L0 + L1 ) / 2.0 ;
    INPUTS_SPECTRO.LAMBIN_LIST[l] = LBIN;

    if ( l > 0 && fabs(LASTBIN-LBIN)>0.001 ) 
      { INPUTS_SPECTRO.FORMAT_MASK = 2; } // write LAMMIN & LAMMAX

    LASTBIN=LBIN; 
  }

  // xxx INPUTS_SPECTRO.FORMAT_MASK = 1; // xxxx REMOVE

  // set global min & max
  INPUTS_SPECTRO.LAM_MIN = INPUTS_SPECTRO.LAMMIN_LIST[0] ;
  INPUTS_SPECTRO.LAM_MAX = INPUTS_SPECTRO.LAMMAX_LIST[NBL-1] ;

  // read ZP and SQSIGSKY for each Texpose.
  // They are stored in fits file as float, but array is double,
  // so use ZP_f and SQ_f as intermediate array.

  float *ZP_f = (float*)malloc( NBL * sizeof(float) ) ;
  float *SQ_f = (float*)malloc( NBL * sizeof(float) ) ;
  for(t=0; t < NBT; t++ ) {
    icol++ ;
    fits_read_col_flt(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1E, 
		      ZP_f, &anynul, &istat );
    sprintf(c1err,"read ZP  column" );
    snfitsio_errorCheck(c1err, istat);

    icol++ ;
    fits_read_col_flt(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1E, 
		      SQ_f, &anynul, &istat );
    sprintf(c1err,"read SQSIGSKY column" );
    snfitsio_errorCheck(c1err, istat);

    for(l=0; l <NBL; l++ ) {
      INPUTS_SPECTRO.ZP[l][t]       = (double)ZP_f[l] ;
      INPUTS_SPECTRO.SQSIGSKY[l][t] = (double)SQ_f[l] ;
      
      /*
      L0 = INPUTS_SPECTRO.LAMMIN_LIST[l] ;
      L1 = INPUTS_SPECTRO.LAMMAX_LIST[l] ;
      Texpose = INPUTS_SPECTRO.TEXPOSE_LIST[t] ;
      if ( fabs(L0-4210.) < 20.0 ) {
	printf(" xxx %.1f-%.1f  Texpose=%4d  ZP=%.3f SQSIG=%.3f \n",
	       L0, L1, (int)Texpose, ZP_f[l], SQ_f[l] );
	fflush(stdout);
      }
      */

    } // end l loop over lambda
  } // end t loop over Texpose

  free(ZP_f);  free(SQ_f);

  // ---------------------------------------------------
  // ---------------------------------------------------
  // read 2nd table of lambda range vs. SYN_FILTER_SPECTROGRAPH
  // ---------------------------------------------------
  // ---------------------------------------------------

  float LAMMIN_f[MXFILTINDX], LAMMAX_f[MXFILTINDX];
  int ifilt ;
  char *cName[MXFILTINDX] ;

  sprintf(TBLname, "SYN_FILTER_SPECTROGRAPH" );

  extver = istat = 0 ;  hdutype = BINARY_TBL ;
  fits_movnam_hdu( fp, hdutype, TBLname, extver, &istat);
  sprintf(c1err,"movnam to %s table (hdutype=%d)",  TBLname, hdutype ) ;
  snfitsio_errorCheck(c1err, istat);

  FIRSTELEM = FIRSTROW = 1 ;  anynul=istat=0 ;
  NROW = strlen(INPUTS_SPECTRO.SYN_FILTERLIST_BAND);
  for(ifilt=0; ifilt<NROW; ifilt++ )  { 
    cName[ifilt] = INPUTS_SPECTRO.SYN_FILTERLIST_NAME[ifilt] ; 
    cName[ifilt][0] = 0 ;
    INPUTS_SPECTRO.SYN_FILTERLIST_LAMMIN[ifilt] = -9.0 ;
    INPUTS_SPECTRO.SYN_FILTERLIST_LAMMAX[ifilt] = -9.0 ;
  }


  icol = 1 ;
  fits_read_col_str(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_A, 
		    &cName[0], &anynul, &istat );
  sprintf(c1err,"read SYN_FILTERLIST_NAME column" );
  snfitsio_errorCheck(c1err, istat);

  icol = 2 ;
  fits_read_col_flt(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1E, 
		    LAMMIN_f, &anynul, &istat );
  sprintf(c1err,"read SYN_FILTERLIST_LAMMIN  column " );
  snfitsio_errorCheck(c1err, istat);

  icol = 3 ;
  fits_read_col_flt(fp, icol, FIRSTROW, FIRSTELEM, NROW, NULL_1E, 
		    LAMMAX_f, &anynul, &istat );
  sprintf(c1err,"read SYN_FILTERLIST_LAMMAX  column" );
  snfitsio_errorCheck(c1err, istat);

  
  // note this is a sparse "ifilt" over SYN_FILTERLIST,
  // and not over all kcor filters.
  for(ifilt=0 ; ifilt < NROW; ifilt++ ) {
    INPUTS_SPECTRO.SYN_FILTERLIST_LAMMIN[ifilt] = LAMMIN_f[ifilt] ;
    INPUTS_SPECTRO.SYN_FILTERLIST_LAMMAX[ifilt] = LAMMAX_f[ifilt] ;

    /*
    printf(" xxx '%s' : LAMRANGE = %.1f to %.1f \n"
	   ,INPUTS_SPECTRO.SYN_FILTERLIST_NAME[ifilt]
	   ,INPUTS_SPECTRO.SYN_FILTERLIST_LAMMIN[ifilt]
	   ,INPUTS_SPECTRO.SYN_FILTERLIST_LAMMAX[ifilt] );  */
  }

  // ------------------------------------------
  // close fits file
 FCLOSE:
  istat = 0 ;
  fits_close_file(fp, &istat);
  sprintf(c1err, "Close Spectrograph FITS file"  );
  snfitsio_errorCheck(c1err, istat);


  return ;

} // end read_spectrograph_fits


// ====================================================
double getSNR_spectrograph(int ILAM, double TEXPOSE_S, double TEXPOSE_T,
			   double GENMAG, double *ERRFRAC_T) {

  // Return SNR for inputs
  //  + SPECTROGRAPH wavelength bin (ILAM)
  //  + search exposure time (TEXPOSE_S)
  //  + template exposure time (TEXPOSE_T)
  //  + magnitude in wavelength bin (GENMAG)
  //
  // *ERRFRAC_T  = sigma_template/FluxErrTot
  //  is the fraction of error associated with template noise.
  //  Used externally to generate correlated template noise.
  //
  // If Texpose is outside valid range, abort.
  // SQSIGSKY is returned as well.
  //
  // Feb 2 2017: fix awful bug and scale template noise to search-zp

  int OPT_INTERP=1;
  int NBT  = INPUTS_SPECTRO.NBIN_TEXPOSE ;
  double SNR, ZP_S, ZP_T, arg, SQ_S, SQ_T, Flux, FluxErr ;
  double LAMAVG = INPUTS_SPECTRO.LAMAVG_LIST[ILAM] ;
  double Tmin   = INPUTS_SPECTRO.TEXPOSE_LIST[0] ;
  double Tmax   = INPUTS_SPECTRO.TEXPOSE_LIST[NBT-1] ;
  char fnam[] = "getSNR_spectrograph" ;
  char errmsg_ZP_S[] = "getSNR_spectrograph(ZP_S)";
  char errmsg_ZP_T[] = "getSNR_spectrograph(ZP_T)";
  char errmsg_SQ_S[] = "getSNR_spectrograph(SQ_S)";
  char errmsg_SQ_T[] = "getSNR_spectrograph(SQ_T)";

  // -------------- BEGIN --------------

  SNR = SQ_S = SQ_T = 0.0 ;

  if ( TEXPOSE_S < Tmin  || TEXPOSE_S > Tmax ) {
    sprintf(c1err,"Invalid TEXPOSE_S = %f", TEXPOSE_S );
    sprintf(c2err,"Valid TEXPOSE_S range: %.2f to %.2f \n", Tmin, Tmax);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  // interpolate ZP(Texpose) and SQSIG(Texpose)
  ZP_S = interp_1DFUN (OPT_INTERP, TEXPOSE_S, NBT, 
		       INPUTS_SPECTRO.TEXPOSE_LIST,
		       INPUTS_SPECTRO.ZP[ILAM], errmsg_ZP_S );
  
  SQ_S = interp_1DFUN (OPT_INTERP, TEXPOSE_S, NBT, 
		       INPUTS_SPECTRO.TEXPOSE_LIST,
		       INPUTS_SPECTRO.SQSIGSKY[ILAM], errmsg_SQ_S );
  
  if ( TEXPOSE_T > 0.01 ) {
    ZP_T = interp_1DFUN (OPT_INTERP, TEXPOSE_T, NBT, 
		       INPUTS_SPECTRO.TEXPOSE_LIST,
		       INPUTS_SPECTRO.ZP[ILAM], errmsg_ZP_T );

    SQ_T = interp_1DFUN (OPT_INTERP, TEXPOSE_T, NBT, 
			 INPUTS_SPECTRO.TEXPOSE_LIST,
			 INPUTS_SPECTRO.SQSIGSKY[ILAM], errmsg_SQ_T );

    // Feb 2 2017: 
    //  scale template noise to search-image exposure. Nominaly
    //  the scaling would be TEXPOSE_S/TEXPOSE_T, but the noise
    //  has a more complex function; hence use the ZP to scale
    //  the template noise.  
    //   SQNOISE_SCALE = FLUXSCALE^2 = 10^( 0.8 * ZPdif )
    SQ_T *= pow( TEN, 0.8*(ZP_S-ZP_T) ) ;
     
  }


  arg     = -0.4*(GENMAG-ZP_S);
  Flux    = pow(TEN,arg) ;      // in p.e.
  FluxErr = sqrt(SQ_S + SQ_T + Flux);

  *ERRFRAC_T = sqrt(SQ_T)/FluxErr ; // Oct 28 2016

  SNR = Flux/FluxErr ;  // true SNR

  return(SNR);

} // end getSNR_spectrograph
