/* =================================================
  June, 2011  R.Kessler
  Separate snhost.h from the snhost.c file

  Feb 5 2015: int GALID -> long long GALID
       
  Aug 18 2015
    New struct HOSTLIB_ZPHOTEFF

  Sep 28 2015:  increase array bounds for WFIRST studies:
     NMAGPSF_HOSTLIB -> 8 (was 5)
     MXVAR_HOSTLIB   -> 200 (was 100)

 Feb 21 2016:  MINLOGZ_HOSTLIB -> -2.3 (was -2.0)

 May 22 2017: NMAGPSF_HOSTLIB->9 (was 8)

 Dec 30 2017: MALLOCSIZE_HOSTLIB -> 40000 (was 10000)

 Jan 15 2018:  MINLOGZ_HOSTLIB -> -2.523 (was -2.3)

 May 10 2018:  extend max z from 3.16 to 4.0
    NZPTR_HOSTLIB -> 320 (was 280)
    MAXLOGZ_HOSTLIB -> 0.6 (was 0.3)

 Feb 4 2019: add DLR and d_DLR

==================================================== */

#define HOSTLIB_MSKOPT_USE           1 // internally set if HOSTLIB_FILE
#define HOSTLIB_MSKOPT_GALMAG        2 // get host mag & noise in SN aperture
#define HOSTLIB_MSKOPT_SNMAGSHIFT    4 // adust SN mag from wgtmap
#define HOSTLIB_MSKOPT_SN2GAL_RADEC  8 // transfer SN coords to galaxy coords
#define HOSTLIB_MSKOPT_SN2GAL_Z     16 // transfer SN Z to ZTRUE
#define HOSTLIB_MSKOPT_USEONCE      32 // use host galaxy only once
#define HOSTLIB_MSKOPT_USESNPAR     64 // use SN color & shape from hostlib
#define HOSTLIB_MSKOPT_VERBOSE     256 // print extra info to screen
#define HOSTLIB_MSKOPT_DEBUG       512 // fix a=2, b=1, rotang=0 
#define HOSTLIB_MSKOPT_DUMP       1024 // screen-dump for each host 

#define HOSTLIB_1DINDEX_ID 10    // ID for 1DINDEX transformations

#define MXCHAR_LINE_HOSTLIB 400  // max number of chars per HOSTLIB line
#define MXVAR_HOSTLIB       200  // max number of variables (NVAR:) in HOSTLIB
#define MXVAR_WGTMAP_HOSTLIB 10  // max no. weight-map variables
#define MXWGT_HOSTLIB      5000  // max number of WGT: keys
#define MXCHECK_WGTMAP     1000  // max no. galaxies to check wgt map
#define MALLOCSIZE_HOSTLIB 40000 // incremental size of internal HOSTLIB array
#define MXCOMMENT_HOSTLIB  40    // max number of lines for README file
#define MXGauss2dTable     200   // max length of Gauss2d table
#define NVAR_Gauss2d       3     // Number of variables in Gauss2d table
#define MXBIN_ZPHOTEFF      100  // 

#define NSERSIC_TABLE        50    // number of integral tables
#define SERSIC_INDEX_MIN   0.15
#define SERSIC_INDEX_MAX   8.00    // increase from 5 (6/24/2015)
#define MXSERSIC_HOSTLIB      9    // max number of summed profiles per host
#define NBIN_RADIUS_SERSIC  200    // Number of R/Re bins to store integrals
#define MAXRADIUS_SERSIC   100.0   // max R/Re value for integ table
#define MINRADIUS_SERSIC  1.0E-4   // min R/Re value for integ table

#define NRBIN_GALMAG        100    // No. of radius bins for Galmag 
#define NTHBIN_GALMAG        36    // No. theta bins for galmag
#define MXBIN_SERSIC_bn     2000   // max bins in Sersic_bn file
 
// hard wire logarithmic z-bins
#define NZPTR_HOSTLIB      320     // number of Z-pointers of hash table
#define DZPTR_HOSTLIB      0.01    // logz-binning for Z-pointers
#define MINLOGZ_HOSTLIB   -2.523   // zmin = 0.003
#define MAXLOGZ_HOSTLIB    0.61    // zmax = 4.07

#define NMAGPSF_HOSTLIB    9    // number of aperture mags vs. PSF to compute
#define DEG_ARCSEC    1./3600.  // 1 arcsec in deg.
#define DEBUG_WGTFLUX2    0.0    // fix 2nd WGTFLUX if non-zero

#define MXUSE_SAMEGAL 50     // max number of times to re-use hostGal
                             // with MINDAYSEP_SAMEGAL option

// define required keys in the HOSTLIB

#define HOSTLIB_VARNAME_GALID     "GALID"  // required 
#define HOSTLIB_VARNAME_ZTRUE     "ZTRUE"  // required

// define optional keys
#define HOSTLIB_VARNAME_ZPHOT        "ZPHOT"
#define HOSTLIB_VARNAME_ZPHOT_ERR    "ZPHOT_ERR" 
#define HOSTLIB_VARNAME_LOGMASS      "LOGMASS"  // log10(Mgal/Msolar) 2/2014
#define HOSTLIB_VARNAME_LOGMASS_ERR  "LOGMASS_ERR" 
#define HOSTLIB_VARNAME_RA           "RA"  
#define HOSTLIB_VARNAME_DEC          "DEC" 
#define HOSTLIB_VARNAME_RA_HOST      "RA_HOST"  
#define HOSTLIB_VARNAME_DEC_HOST     "DEC_HOST" 
#define HOSTLIB_VARNAME_RA_GAL       "RA_GAL"  
#define HOSTLIB_VARNAME_DEC_GAL      "DEC_GAL" 
#define HOSTLIB_VARNAME_ANGLE        "a_rot"    // rotation angle
#define HOSTLIB_VARNAME_FIELD        "FIELD" 
#define HOSTLIB_MAGOBS_SUFFIX        "_obs"     // key = [filt]$SUFFIX
#define HOSTLIB_SNPAR_UNDEFINED  -9999.0 


// for SNMAGSHIFT, allow hostlib param instead of wgtmap
#define HOSTLIB_VARNAME_SNMAGSHIFT  "SNMAGSHIFT" 

// define ascii files with tables needed for numerical computation
#define FILENAME_Gauss2d    "$SNDATA_ROOT/simlib/Gauss2dIntegrals.dat" 
#define FILENAME_Sersic_bn  "$SNDATA_ROOT/simlib/Sersic_bn.dat" 


struct HOSTLIB_DEF {
  char FILENAME[MXPATHLEN] ; // full file name of HOSTLIB
  int  GZIPFLAG;

  int  NGAL_READ  ; // total number of galaxies read
  int  NGAL_STORE ; // number of GAL entries read and stored

  int  NVAR_REQUIRED ;
  int  NVAR_OPTIONAL ;
  int  NVAR_ALL    ; // total no, variables in HOSTLIB
  int  NVAR_STORE  ; // NVAR wgtmap + required + optional

  char VARNAME_REQUIRED[MXVAR_HOSTLIB][40];
  char VARNAME_OPTIONAL[MXVAR_HOSTLIB][40];
  char VARNAME_ALL[MXVAR_HOSTLIB][40];
  char VARNAME_STORE[MXVAR_HOSTLIB][40];  
  
  int  IVAR_ALL[MXVAR_HOSTLIB];  // [sparse store index] = ALL-ivar

  double VALMIN[MXVAR_HOSTLIB];  // vs. IVAR_STORE
  double VALMAX[MXVAR_HOSTLIB]; 

  int  NVAR_SNPAR  ; // subset of SN parameters (e.g., shape, color ...)
  char VARSTRING_SNPAR[100] ;     // string of optional SN par names 
  int  IS_SNPAR_OPTIONAL[MXVAR_HOSTLIB];   // flag subset that are SN par
  int  FOUND_SNPAR_OPTIONAL[MXVAR_HOSTLIB] ;
  int  IS_SNPAR_STORE[MXVAR_HOSTLIB];   // flag subset that are SN par

  // define pointers used to malloc memory with MALLOCSIZE_HOSTLIB
  double *VALUE_ZSORTED[MXVAR_HOSTLIB];  // sorted by redshift
  double *VALUE_UNSORTED[MXVAR_HOSTLIB]; // same order as in HOSTLIB
  int     SORTFLAG ; // 1=> sorted

  char **FIELD_UNSORTED ;
  char **FIELD_ZSORTED ;

  int MALLOCSIZE ;

  // pointers to stored variables
  int IVAR_GALID ;
  int IVAR_ZTRUE  ;
  int IVAR_ZPHOT ;
  int IVAR_ZPHOT_ERR  ;
  int IVAR_LOGMASS ;
  int IVAR_LOGMASS_ERR ;
  int IVAR_RA ;
  int IVAR_DEC ; 
  int IVAR_ANGLE ;  // rot angle of a-axis w.r.t. RA
  int IVAR_FIELD ;                  // optional FIELD key (Sep 16 2015)
  int IVAR_a[MXSERSIC_HOSTLIB];   // semi-major  half-light
  int IVAR_b[MXSERSIC_HOSTLIB];   // semi-minor 
  int IVAR_w[MXSERSIC_HOSTLIB];   // weight
  int IVAR_n[MXSERSIC_HOSTLIB];   // Sersic index
  int IVAR_MAGOBS[MXFILTINDX] ;     // pointer to oberver-mags
  int IVAR_WGTMAP[MXVAR_HOSTLIB] ;  // wgtmap-ivar vs [ivar_STORE]
  int IVAR_STORE[MXVAR_HOSTLIB]  ;  // store-ivar vs [ivarmap]
  int NFILT_MAGOBS;  // NFILT with host mag info read

  char filterList[MXFILTINDX]; // filter list for gal-mag

  // redshift information
  double ZMIN,ZMAX ;
  double ZGAPMAX ; // max z-gap in library
  double ZGAPAVG ; // avg z-gap in library
  double Z_ATGAPMAX[2];  //  redshift at max ZGAP (to find big holes)
  int   IZPTR[NZPTR_HOSTLIB]; // pointers to nearest z-bin with .01 bin-size
  int   MINiz, MAXiz ;        // min,max valid iz arg for IZPTR

  int NLINE_COMMENT ;
  char COMMENT[MXCOMMENT_HOSTLIB][80] ; // comment lines for README file.

  // PSF-aperture info
  double Aperture_Radius[NMAGPSF_HOSTLIB+1]; // integ. radius for each PSF
  double Aperture_PSFSIG[NMAGPSF_HOSTLIB+1]; // fixed list of sigma-PSFs (asec)
  double Aperture_Rmax ;   // max radius (arsec) to integrage gal flux
  double Aperture_Rbin ;   // radial integration bins size (arcsec)
  double Aperture_THbin ;  // azim. integration binsize (radians)

  // pre-computed 2d-Gaussian integrals
  int    NGauss2d ;        // Number of Gauss2d entries
  int    NBIN_Gauss2dRadius ;
  int    NBIN_Gauss2dSigma ;
  double Gauss2dTable[NVAR_Gauss2d+1][MXGauss2dTable] ;
  double Gauss2dRadius[3]; // binsize, min, max
  double Gauss2dSigma[3];  // binsize, min, max

  // pre-computed cos and sin to speed gal-flux integration
  double Aperture_cosTH[NTHBIN_GALMAG+1] ;
  double Aperture_sinTH[NTHBIN_GALMAG+1] ;

} HOSTLIB ;


struct SAMEHOST_DEF {

  int REUSE_FLAG ;          // 1-> re-use host
  unsigned short  *NUSE ;     // number of times each host is used.

  // define array to store all PEAKMJDs for each host; allows re-using
  // host after NDAYDIF_SAMEGAL. Note 2-byte integers to save memory
  unsigned short **PEAKDAY_STORE ; // add PEAKMJD_STORE_OFFSET to get PEAKMJD
  int PEAKMJD_STORE_OFFSET ;       // min generated PEAKMJD

} SAMEHOST ;

// Sersic quantities to define galaxy profile
// these are all defined during init
struct SERSIC_PROFILE_DEF {
  int  NDEF ;    // number of defined Sersic/profile components  

  char VARNAME_a[MXSERSIC_HOSTLIB][12];     // name of major axis; i.e, a1
  char VARNAME_b[MXSERSIC_HOSTLIB][12];     // name of minor axis; i.e, b1
  char VARNAME_w[MXSERSIC_HOSTLIB][12];     // name of weight
  char VARNAME_n[MXSERSIC_HOSTLIB][12];     // name of index

  int  IVAR_a[MXSERSIC_HOSTLIB];        // pointer to 'a' values
  int  IVAR_b[MXSERSIC_HOSTLIB];        // 
  int  IVAR_w[MXSERSIC_HOSTLIB];        // 
  int  IVAR_n[MXSERSIC_HOSTLIB];        // 

  double  FIXn[MXSERSIC_HOSTLIB];

  int    NFIX; // number of Sersic profiles with fixed index
  double FIX_VALUE[MXSERSIC_HOSTLIB];     // list of fixed Sersic indices
  char   FIX_NAME[MXSERSIC_HOSTLIB][12];  // list of fixed Sersic indices

} SERSIC_PROFILE ;


// Sersic integral tables to speed calculations
struct SERSIC_TABLE_DEF {
  int     TABLEMEMORY ;  // total memory of integral tables (bytes)
  double  INVINDEX_MIN ; // min 1/n 
  double  INVINDEX_MAX ; // max 1/n
  double  INVINDEX_BIN ; // binsize of 1/n

  double  inv_n[NSERSIC_TABLE+1] ; // list of 1/n
  double  n[NSERSIC_TABLE+1] ;     // i.e, n=4 for deVauc, n=1 for exponent ...
  double  bn[NSERSIC_TABLE+1] ;   // calculated b_n values

  double *INTEG_CUM[NSERSIC_TABLE+1];   // cumulative integral per Sersic index
  double  INTEG_SUM[NSERSIC_TABLE+1];   // store total integral for each index

  int  BIN_HALFINTEGRAL[NSERSIC_TABLE+1]; // bin where integral is total/2

  int  NBIN_reduced ;  // number of reduced R/Re bins
  double *reduced_logR ;     // list of R/Re upper-interal limit
  double  reduced_logRmax ;  // max R/Re in table
  double  reduced_logRmin ;  // max R/Re in table
  double  reduced_logRbin ;  // bin size  of R/Re

  // define table-grid of b_n vs. n read from ascii file FILENAME_SERSIC_BN
  int    Ngrid_bn ;
  double grid_n[MXBIN_SERSIC_bn] ;
  double grid_bn[MXBIN_SERSIC_bn] ;

} SERSIC_TABLE ;


struct HOSTLIB_ZPHOTEFF_DEF {
  int    NZBIN ;
  double ZTRUE[MXBIN_ZPHOTEFF] ;
  double EFF[MXBIN_ZPHOTEFF] ;
} HOSTLIB_ZPHOTEFF ; 

// define the weight  map .. 
struct HOSTLIB_WGTMAP_DEF {
  // xx  int   NGRID_TOT ; // total number of GRID bins
  // xx  int   NVAR     ; // number of WGTMAP variables

  // GRID value vs. [ivar][igrid]
  //xx  double **GRIDVAL ;

  // parameters describing each WGT
  double WGTMAX ; // max weight for entire  wgtmap
  //xx  double GRID_WGT[MXWGT_HOSTLIB];         // weight at each GRID point
  //xxdouble GRID_SNMAGSHIFT[MXWGT_HOSTLIB];  // mag-shift at each GRID  point

  char VARNAME[MXVAR_HOSTLIB][40]; 
  int  ISTAT ;    // non-ZERO => wgtmap has been read
 
  double *WGTSUM ;      // cumulative sum of weights over entire HOSTLIB
  double *WGT ;         // wgt for each hostlib entry
  double *SNMAGSHIFT ;  // SN mag shift at for each hostlib entry

  // define  arrays to store list of GALIDs to check wgtmap interpolation
  int      NCHECKLIST ;
  int       CHECKLIST_IGAL[MXCHECK_WGTMAP] ;  // sparse pointer
  long long CHECKLIST_GALID[MXCHECK_WGTMAP] ; // absolute GALID
  double    CHECKLIST_ZTRUE[MXCHECK_WGTMAP] ;
  double    CHECKLIST_WGT[MXCHECK_WGTMAP] ;
  double    CHECKLIST_SNMAG[MXCHECK_WGTMAP] ;

  struct  GRIDMAP  GRIDMAP ;

} HOSTLIB_WGTMAP ;



// define structure to hold information for one event ...
// gets over-written for each generated SN
struct SNHOSTGAL {

  int   IGAL  ;     // sequential sparse galaxy index 
  int   IGAL_SELECT_RANGE[2] ; // range to select random IGAL

  long long GALID ;   // Galaxy ID from library
  //  int    GALID ; 

  // redshift info
  double ZGEN  ;     // saved ZSN passed to driver
  double ZTRUE ;     // host galaxy redshift 
  double ZDIF ;      // zSN(orig) - zGAL, Nov 2015
  double ZPHOT, ZPHOT_ERR ;     // photoZ of host
  double ZSPEC, ZSPEC_ERR ;     // = zSN or z of wrong host
  double PEAKMJD ;

  // Sersic profiles for this host
  double  SERSIC_INDEX ; // Sersic 'n' used to get SN pos
  double  SERSIC_a[MXSERSIC_HOSTLIB]  ;
  double  SERSIC_b[MXSERSIC_HOSTLIB]  ;
  double  SERSIC_n[MXSERSIC_HOSTLIB]  ;
  double  SERSIC_w[MXSERSIC_HOSTLIB]  ;
  double  SERSIC_wsum[MXSERSIC_HOSTLIB] ;
  double  SERSIC_bn[MXSERSIC_HOSTLIB] ;

  // coordinate info

  double reduced_R ;     // reduced R/Re (randomly chosen)
  double phi ;           // randomly chosen azim. angle, radians  

  double a_SNGALSEP_ASEC ;  // angle-coord along major axis
  double b_SNGALSEP_ASEC ;  // idem for minor axis

  double RA_GAL_DEG ;       // Galaxy sky coord from library (DEG)
  double DEC_GAL_DEG ;  
  double RA_SN_DEG ;            // SN sky coord (DEG)
  double DEC_SN_DEG ;   
  double RA_SNGALSEP_ASEC ;     // SN-galaxy sep in RA, arcsec
  double DEC_SNGALSEP_ASEC ;    // idem in DEC
  double SNSEP ;        // SN-gal sep, arcsec
  double DLR ;          // directional light radius
  double DDLR;          // SNSEP/DLR (following Gupta 2016)

  // aperture-mag info
  double SB_MAG[MXFILTINDX] ;  // surface brightness mag in 1 sq-arcsec
  double SB_FLUX[MXFILTINDX] ;

  double GALMAG_TOT[MXFILTINDX];                 // Dec 20 2018
  double GALMAG[MXFILTINDX][NMAGPSF_HOSTLIB+1] ; // mag per PSF bin
  double GALFRAC[NMAGPSF_HOSTLIB+1]; // gal light-frac in each aperture
  double GALFRAC_SBRADIUS[NMAGPSF_HOSTLIB+1]; // gal light-frac in SB radius
  double WGTMAP_SNMAGSHIFT ;        // SN mag shift from wgtmap
  double WGTMAP_WGT ;               // selection weight

  // log10 of Mgal/Msolar
  double LOGMASS, LOGMASS_ERR ;

  // parameters used to interpolate selection WGT
  double WGTMAP_VALUES[MXVAR_HOSTLIB]; 

  // random numbers (0-1)
  double FlatRan1_GALID ;
  double FlatRan1_radius[2] ;
  double FlatRan1_phi ;  // random 0-1, not 0 to 2*PI

} SNHOSTGAL ;


// store arbitrary extra variables to write out to data file
struct SNTABLEVAR_DEF {
  int    NOUT ;  // number of variables to write out
  int    IVAR_STORE[MXVAR_HOSTLIB] ;
  char   NAME[MXVAR_HOSTLIB][40];

  // values updated for each event
  double VALUE[MXVAR_HOSTLIB] ;   
} HOSTLIB_OUTVAR_EXTRA ;


time_t TIME_INIT_HOSTLIB[2];

// =====================================
//
// prototype declarations (moved from sntools_host.c, Feb 2013)
//
// =====================================

void   INIT_HOSTLIB(void);  // one-time init
void   init_SNHOSTGAL(void);  // init each event
void   GEN_SNHOST_DRIVER(double ZGEN, double PEAKMJD);
void   GEN_SNHOST_GALID(double ZGEN);
void   GEN_SNHOST_POS(int IGAL);
void   TRANSFER_SNHOST_REDSHIFT(int IGAL);
void   GEN_SNHOST_GALMAG(int IGAL);
void   GEN_SNHOST_ZPHOT(int IGAL);
int    USEHOST_GALID(int IGAL) ;
void   FREEHOST_GALID(int IGAL) ;
void   checkAbort_noHOSTLIB(void) ;
void   checkAbort_HOSTLIB(void) ;

void   STORE_SNHOST_MISC(int IGAL);
double modelPar_from_SNHOST(double parVal_orig, char *parName);
void   DEBUG_1LINEDUMP_SNHOST(void) ;
void   DUMP_SNHOST(void);
void   initvar_HOSTLIB(void);
void   init_OPTIONAL_HOSTVAR(void) ;
void   init_REQUIRED_HOSTVAR(void) ;
int    load_VARNAME_STORE(char *varName) ;
void   open_HOSTLIB(FILE **fp);
void   rdwgtmap_HOSTLIB(void);
void   parse_WGTMAP_HOSTLIB(FILE *fp, char *string);
void   parse_WGTMAP_HOSTLIB_LEGACY(FILE *fp, char *string);
void   parse_Sersic_n_fixed(FILE *fp, char *string); 
void   rdhead_HOSTLIB(FILE *fp);
void   checkAlternateVarNames(char *varName) ;
void   rdgal_HOSTLIB(FILE *fp);
void   rdgalRow_HOSTLIB(FILE *fp, int nval, double *values, char *field );
void   summary_snpar_HOSTLIB(void) ;
void   malloc_HOSTLIB(int NGAL);
void   sortz_HOSTLIB(void);
void   zptr_HOSTLIB(void);
void   init_HOSTLIB_WGTMAP(void);
void   init_HOSTLIB_ZPHOTEFF(void);
void   init_GALMAG_HOSTLIB(void);
void   init_Gauss2d_Overlap(void);
void   init_SAMEHOST(void);

void   init_Sersic_VARNAMES(void);
void   init_Sersic_HOSTLIB(void);
void   init_Sersic_integrals(int j);
void   read_Sersic_bn(void);
void   Sersic_names(int j, char *a, char *b, char *w, char *n);
void   get_Sersic_info(int IGAL) ;
void   test_Sersic_interp(void);
double get_Sersic_bn(double n);
void   init_OUTVAR_HOSTLIB(void) ;
void   LOAD_OUTVAR_HOSTLIB(int IGAL) ;
void   copy_VARNAMES_zHOST_to_HOSTLIB_STOREPAR(void);

void   readme_HOSTLIB(void);
void   check_duplicate_GALID(void);
int    IVAR_HOSTLIB(char *varname, int ABORTFLAG);
long long get_GALID_HOSTLIB(int igal);
double get_ZTRUE_HOSTLIB(int igal);
double get_GALFLUX_HOSTLIB(double a, double b);

double interp_GALMAG_HOSTLIB(int ifilt_obs, double PSF ); 
double Gauss2d_Overlap(double offset, double sig);
void   magkey_HOSTLIB(int  ifilt_obs, char *key);
void   set_usebit_HOSTLIB_MSKOPT(int MSKOPT);

void setbit_HOSTLIB_MSKOPT(int MSKOPT) ; // added Jan 2017

void GEN_SNHOST_ZPHOT_from_CALC(int IGAL,    double *ZPHOT, double *ZPHOT_ERR);
void zphoterr_asym(double ZTRUE, double ZPHOTERR, 
		   GENGAUSS_ASYM_DEF *asymGaussPar );

void GEN_SNHOST_ZPHOT_from_HOSTLIB(int IGAL, double *ZPHOT, double *ZPHOT_ERR); 

// END
