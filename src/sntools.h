/*******************************************************
     Created Jan 2005 by R.Kessler 

     Defines: 
      - Generic tools to read input from file
      - error handling routines
      - physical constants.


  Aug 22 2013:  define MODEL_S11DM15

  Aug 28 2013:  define struct SNTOOLS_FITRES to hold FITRES-variables;
                avoids accidental conflicts with other programs.

  Mar 20 2017: remove special chars from  FILTERSTRING_DEFAULT, except
               for '&' to add default SYN_SPECTROGRAPH filter if
               user does not define them.

  Apr 6 2017: define EXEC_CIDMASK()

  Jul 5 2017: add read_SURVEY

  Nov 2017: add XXX_PATH_SNDATA_SIM tools to keep track of
            user-defined output directories for simulations.

  Dec 10 2017: declare SNANA_VERSION here instad of snana.car
               so that it is more accessible. Also define function
               get_SNANA_VERSION.

********************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include "sndata.h"

#define  SNANA_VERSION_CURRENT  "v10_71j"                                  

#define LIGHT_km  2.99792458e5      // speed of light (km/s) 
#define LIGHT_A   2.99792458e18     // speed of light (A/s) 
#define PLANCK    6.6260755e-27     // Planck constant (erg s) 
#define hc        LIGHT_A * PLANCK
#define PC_km     3.085678e13       // parsec (km)
#define TWOPI     6.28318530718
#define RADIAN    TWOPI / 360.0     // added Oct 2010
#define ZAT10PC    2.335e-9         // redshift at 10pc (H0=70)
#define ZMAX_SNANA 4.0              // max snana redshift, Dec 26 2016

#define  CMBapex_l  (double)264.14    // deg (RA galactic coords !!!)
#define  CMBapex_b  (double)48.26     // deg (DECL)
#define  CMBapex_v  (double)371.0     // km/sec

#define FWHM_SIGMA_RATIO  2.3548    // FWHM/sigma for Gaussian 
#define TEN        (double)10.0 
#define LNTEN      (double)2.30259  // ln(10)

#define REJECT_FLAG    -1  
#define ACCEPT_FLAG    +1
#define ERROR     -1  // use these as return args for int functions
#define SUCCESS   +1
#define NULLINT    -9 
#define NULLFLOAT  -9.0 
#define NULLDOUBLE (double)-9.0 
#define NULLSTRING "NULL"
#define NODOUBLE      (double)1.7777E14
#define NOFLOAT       (float)1.7777E14
#define NOINT         555444333
#define BLANK_STRING   ""
#define NOTSET_STRING  "NOTSET" // init str pointers to avoid compile error

#define NULLTYPE    0   // SN type with no TYPE in data base.

#define FLUXCALERR_SATURATE 1.0E8
#define MAG_SATURATE   -7.0   // saturated mag
#define MAG_ZEROFLUX   99.0   // expect flux=0 (e.g., pre-explosion)
#define MAG_NEGFLUX   128.0   // fluctuation results in flux < 0
#define MAG_UNDEFINED 666.0   // model mag wave-range doesn't cover filter
#define MAGERR_UNDEFINED 9.0
#define FLUX_UNDEFINED  -9.0  // undefined model flux 

#define MODEL_STRETCH  1 // single-stretch (use rise/fall fudges for 2-stretch)
#define MODEL_MLCS2k2  3  // MLCS2k2 (Riess, Suarab, Hubert)
#define MODEL_SNOOPY   4  // dm15 model from C.Burns et al, 2011
#define MODEL_S11DM15  5  // hybrid-DM15 model for psnid, Sako et al, 2011
#define MODEL_SALT2    6  // SNLS-SALT-II model
#define MODEL_SIMSED   7  // set of SED sequences (e.g., sim-explosion models)
#define MODEL_BYOSED   8  // build-your-own SED model
#define MODEL_NON1ASED   10  // obs-frame NONIA from SED
#define MODEL_NON1AGRID  11  // obs-frame NONIA from mag-grid (Mar 2016)
#define MODEL_LCLIB      12  // light curve library (July 2017)
#define MODEL_FIXMAG     20  // constant/fixed mag (default is mag=20)
#define MODEL_RANMAG     21  // constant/fixed mag per LC, random each event
#define MXMODEL_INDEX    40



// keep this in sync with the fortran FILTDEF_STRING
// Oct 22 2015: add 27 special chars to hack an IFU
#define FILTERSTRING_DEFAULT  " ugrizYJHK UBVRIXy0123456789 abcdef ACDEFGLMNOPQSTWZ hjklmnopqstvwx &"

#define PRIVATE_MODELPATH_NAME "SNANA_MODELPATH"  // name of optional env

char FILTERSTRING[100] ;

// define variables for random number list
#define MXLIST_RAN      4  // max number of lists
#define MXSTORE_RAN  1000  //  size of each RANLIST (for each event)
double  RANSTORE8[MXLIST_RAN+1][MXSTORE_RAN] ;
int     NLIST_RAN;         // Number of lists
int     NSTORE_RAN[MXLIST_RAN+1] ;
double  RANFIRST[MXLIST_RAN+1], RANLAST[MXLIST_RAN+1]; // for syncing.

// errmsg parameters 
char c1err[200];   // for kcorerr utility 
char c2err[200];   // for kcorerr utility 
char BANNER[200];
int  EXIT_ERRCODE;  // program error code set by program (Jan 2019)

#define MXARGV 100
int  NARGV_LIST;
char ARGV_LIST[MXARGV][200];
int  USE_ARGV_LIST[MXARGV];  // 1 => line arg was used, 0=> not used.

// ---------
// generic DUMP_STRING for any function
#define MXCID_DUMP 10
#define DUMPFLAG_noABORT    1
#define DUMPFLAG_withABORT  2
struct {
  char  FUNNAME[80] ; // name of funtion to dump
  int   NCID ;        // number of CIDs to dump
  char  CCIDLIST[MXCID_DUMP][40];
  int   NFILT;
  char  FILTLIST[MXFILTINDX];
  int   ABORTFLAG ;  // 1 --> abort after dumping all CIDLIST

  double MJDRANGE[2] ;

  // number of DONE filters per CID
  int NFILT_DONE[MXCID_DUMP] ;

} DUMP_STRING_INFO ;

// Store GaussIntegrals from 0 to x 
struct {
  int    INIT_FLAG;
  int    NBIN_XMAX ;
  double XMAX;
  double XMAX_LIST[MXSTORE_RAN];
  double GINT_LIST[MXSTORE_RAN];
} GAUSS_INTEGRAL_STORAGE ;

// variables for Landolt transformations (UBVRI,BX)
#define NFILT_LANDOLT 6
double LANDOLT_MAGPRIMARY[6];     // UBVRI,X synthetic mag for primary ref
double LANDOLT_COLOR_VALUE[6];   // Marriner's k0 - k4
double LANDOLT_COLOR_ERROR[6];   // errors on above


// offsets for quickly reducing n-dim indices to 1d-index
#define MXMAP_1DINDEX 200 // max number of 1d index maps to store
#define MXDIM_1DINDEX 10  // max number of dimenstions


int OFFSET_1DINDEX[MXMAP_1DINDEX][MXDIM_1DINDEX] ;
int NPT_PERDIM_1DINDEX[MXMAP_1DINDEX][MXDIM_1DINDEX] ;


typedef struct GRIDMAP {
  int     ID;        // 
  int     NDIM;     // Number of dimensions
  int    *NBIN;     // Number of bins in each dimension
  double *VALMIN;   // min value in each dimension
  double *VALMAX;   // max value in each dimension
  double *VALBIN;   // binsize in each dimension
  double *RANGE;    // max-min in each dimension

  int    NFUN ;        // Number of stored functions
  double **FUNVAL ;    // function value at each grid point 
  double  *FUNMIN ;    // min fun-val per function
  double  *FUNMAX ;    // max fun-val per function
  int    *INVMAP;      // covert multi-D indices into 1D index
  int  NROW;          // number or rows read from file 
  int  OPT_EXTRAP;   // 1=>snap outside values to edge
  char VARLIST[80]; // comma-sep list of variables (optional to fill)
} GRIDMAP ;

typedef struct GRIDMAP1D {
  int NBIN ;
  double  *XVAL, *YVAL ;
} GRIDMAP1D ;

#define IDGRIDMAP_SIMEFFMAP              8  // for MLCS-AV prior
#define IDGRIDMAP_HOSTLIB_WGTMAP        20  // HOSTLIB weight map
#define IDGRIDMAP_SPECEFF_OFFSET        30  // id = OFFSET + imap
#define IDGRIDMAP_zHOST_OFFSET          40  // id = OFFSET + imap
#define IDGRIDMAP_PHOTPROB_OFFSET       50  // id = OFFSET + imap
#define IDGRIDMAP_FLUXERRMODEL_OFFSET  100  // id = OFFSET + imap

// simeff 
#define MXGENVAR_SIMEFFMAP   10

struct SIMEFFMAP_DEF {
  int NGENVAR ; // number of variables to specify map
  char   VARNAME[MXGENVAR_SIMEFFMAP][20] ;  // variable name
  char   VARSCALE[MXGENVAR_SIMEFFMAP][8] ;  // LIN, LOG or INV
  int    IFLAGSCALE[MXGENVAR_SIMEFFMAP];    // 1  , 10,    -1
  int    NBINTOT;
  int    NBIN[MXGENVAR_SIMEFFMAP];
  double VARMIN[MXGENVAR_SIMEFFMAP] ;
  double VARMAX[MXGENVAR_SIMEFFMAP] ;
  double EFFMAX ; // max efficiency in map 

  double **TMPVAL ; // temp memory to read effic. map
  double  *TMPEFF ; // idem
} SIMEFFMAP ;

struct GRIDMAP  SIMEFF_GRIDMAP ;


// Jun 8 2018: move GENGAUSS_ASYM from snlc_sim.h to here
typedef struct  {
  char   NAME[80];  // name of variable                       
  double PEAK ;     // peak prob                          
  double SIGMA[2] ; // asymmetric Gaussian sigmas 
  double SKEW[2] ;  // hack-skew; one TrueSigma = SIGMA + SKEW*|x-PEAK| 
  double SKEWNORMAL[3] ; // real skew (for Rachel & Elise, Aug 2016) 
  double RANGE[2] ; // allows truncation 
  int    NGRID ;      // if non-zero, snap to grid

  // Mar 29 2017; add 2nd peak params (for low-z x1)      
  double PROB2;     // prob of generating 2nd peak (default=0.0) 
  double PEAK2;     // location of 2nd peak 
  double SIGMA2[2]; // asym Gaussian sigmas of 2nd peak 
  int  FUNINDEX;    // = NFUN_GENGUASS_ASYM = unique index 

  // Jun 2018
  double RMS; // optionally filled.

} GENGAUSS_ASYM_DEF ;


// Mar 2019: define user-input polynomial typedef with arbitrary order.
#define MXORDER_GENPOLY 20
typedef struct  {
  int ORDER;   // 2nd order means up to x^2
  double COEFF_RANGE[MXORDER_GENPOLY][2]; // range for each coeff.
  char   STRING[200]; // string that was parsed to get COEFF_RANGEs
} GENPOLY_DEF ;


#define MXFILT_REMAP 20
struct  {
  int  NMAP ;
  char MAPSTRING[MXFILT_REMAP][20] ;
  int  IFILTOBS_MAP[MXFILTINDX] ;  // ifiltobs_orig -> ifiltobs_remap
  int  IFILT_MAP[MXFILTINDX] ;     // ifiltobs_orig -> ifilt_remap
} FILTER_REMAP  ;


// structure to hold warnings for old/obsolete inputs that should 
// be replaced.
struct {
  int  NWARN ;
  char VARNAME_OLD[20][80];  // [iwarn][lenvarname]
  char VARNAME_NEW[20][80];  // [iwarn][lenvarname]
} OLD_INPUTS;


#define ADDBUF_PARSE_WORDS 10000
#define MXCHARWORD_PARSE_WORDS 60     // MXCHAR per word
#define MXCHARLINE_PARSE_WORDS 2000   // max chars per line
#define MXWORDLINE_PARSE_WORDS  700   // max words per line
#define MXWORDFILE_PARSE_WORDS 500000 // max words to parse in a file
struct {
  char  FILENAME[MXPATHLEN];
  int   BUFSIZE ;
  int   NWD;
  char **WDLIST;
} PARSE_WORDS ;


#define MXLIST_STRING_UNIQUE 200
struct {
  int  NLIST;
  char SOURCE_of_STRING[200];
  char STRING[MXLIST_STRING_UNIQUE][60];  
} STRING_UNIQUE ;

struct {
  int     LAST_NOBS ;  // to know when a new malloc is needed.
  double *TLIST_SORTED, *MAGLIST_SORTED, *FLUXLIST_SORTED ;
  int    *INDEX_SORT;
} LCWIDTH ;

//double *TLIST_SORTED_WIDTH, *MAGLIST_SORTED_WIDTH ;
//int    *INDEX_SORT_WIDTH ;


// ##############################################################
//
//     functions
//
// ##############################################################



void INIT_SNANA_DUMP(char *STRING);
int  CHECK_SNANA_DUMP(char *FUNNAME, char *CCID, char *BAND, double MJD );
void ABORT_SNANA_DUMP(void) ;

void init_snana_dump__(char *STRING);
int  check_snana_dump__(char *FUNNAME, char *CCID, char *BAND, double *MJD);
void abort_snana_dump__(void);

// ---- utility to REMAP filters for tables -----
void FILTER_REMAP_INIT(char *remapString, char *VALID_FILTERLIST, 
		       int *NFILT_REMAP, 
		       int *IFILTLIST_REMAP, char *FILTLIST_REMAP);
void FILTER_REMAP_FETCH(int IFILTOBS_ORIG, 
			int *IFILTOBS_REMAP, int *IFILT_REMAP);

void filter_remap_init__(char *remapString, char *VALID_FILTERLIST, 
			 int *NFILT_REMAP,
			 int *IFILTLIST_REMAP, char *FILTLIST_REMAP);

void filter_remap_fetch(int *IFILTOBS_ORIG, 
			int *IFILTOBS_REMAP, int *IFILT_REMAP);

int intrac_() ;  // needed by fortran minuit

void get_SNANA_VERSION(char *snana_version); // Dec 2017
void get_snana_version__(char *snana_version);


void print_KEYwarning(int ISEV, char *key_old, char *key_new);
void set_SNDATA(char *key, int NVAL, char *stringVal, double *parVal ) ;
void set_FILTERSTRING(char *FILTERSTRING) ;
void set_EXIT_ERRCODE(int ERRCODE); 
void set_exit_errcode__(int *ERRCODE);

int  IGNOREFILE(char *fileName);
int  ignorefile_(char *fileName);

int strcmp_ignoreCase(char *str1, char *str2) ;


// data functions
void clr_VERSION ( char *version, int prompt );  // remove old *version files
int init_VERSION ( char *version);  // init VERSION_INFO struct 
int init_SNPATH(void);
int init_SNDATA ( void ) ;  // init SNDATA struct

void ld_null(float *ptr, float value);

int rd_SNDATA(void);
int rd_filtband_int   ( FILE *fp, int isn, int i_epoch, int   *iptr ) ;
int rd_filtband_float ( FILE *fp, int isn, int i_epoch, float *fptr ) ;

int rd_sedFlux( char *sedFile, char *sedcomment,
		double DAYrange[2], double LAMrange[2],
		int MXDAY, int MXLAM, int OPTMASK,
		int *NDAY, double *DAY_LIST, double *DAY_STEP,
		int *NLAM, double *LAM_LIST, double *LAM_STEP,
		double *FLUX_LIST, double *FLUXERR_LIST );

int  PARSE_FILTLIST(char *filtlist_string, int *filtlist_array );

void update_covMatrix(char *name, int OPTMASK, int MATSIZE,
		      double (*covMat)[MATSIZE], double EIGMIN, int *istat_cov); 
void update_covmatrix__(char *name, int *OPTMASK, int *MATSIZE,
			double (*covMat)[*MATSIZE], double *EIGMIN,
			int *istat_cov ) ;

int  store_PARSE_WORDS(int OPT, char *FILENAME);
void malloc_PARSE_WORDS(void);
void get_PARSE_WORD(int langFlag, int iwd, char *word);

void init_GENPOLY(GENPOLY_DEF *GENPOLY);
void parse_GENPOLY(char *string, GENPOLY_DEF *GENPOLY, char *callFun );
double eval_GENPOLY(double VAL, GENPOLY_DEF *GENPOLY, char *callFun);
void parse_multiplier(char *inString, char *key, double *multiplier);
void check_uniform_bins(int NBIN, double *VAL, char *comment_forAbort);
void check_argv(void);

double NoiseEquivAperture(double PSFSIG1, double PSFSIG2, double PSFratio);
double noiseequivaperture_(double *PSFSIG1, double *PSFSIG2, double *PSFratio);

double modelmag_extrap(double T, double Tref, 
		       double magref, double magslope, int LDMP);
double modelflux_extrap(double T, double Tref, 
			double fluxref, double fluxslope, int LDMP);

int fluxcal_SNDATA ( int iepoch, char *magfun ) ;
double asinhinv(double mag, int ifilt);


float effective_aperture ( float PSF_sigma, int VBOSE ) ;

int   wr_SNDATA(int IFLAG_WR, int IFLAG_DBUG);

void  wr_HOSTGAL(FILE *fp);

void  wr_SIMKCOR(FILE *fp, int EPMIN, int EPMAX ) ; 

int  wr_filtband_int   ( FILE *fp, char *keyword, 
			 int NINT, int *iptr, char *comment, int opt );
int  wr_filtband_float ( FILE *fp, char *keyword, 
			 int NFLOAT, float *fptr, char *comment, int idec );

int  header_merge(FILE *fp, char *auxheader_file);

int sort_epochs_bymjd(void);

int   WRSTAT ( int wrflag, float value ) ;
// xxx mark delete Jan 23 2018 int   IDTELESCOPE ( char *telescope );

int   Landolt_ini(int opt, float *mag, float *kshift);
int   landolt_ini__(int *opt, float *mag, float *kshift);
int   Landolt_convert(int opt, double *mag_in, double *mag_out);
int   landolt_convert__(int *opt, double *mag_in, double *mag_out);

// simeff utilities (include mangled functions for fortran)
int    init_SIMEFFMAP(char *file, char *varnamesList);
double get_SIMEFFMAP(int OPTMASK, int NVAR, double *GRIDVALS);
void   malloc_SIMEFFMAP(int flag);

int    init_simeffmap__(char *file, char *varnamesList);
double get_simeff__(int *OPTMASK, int *NVAR, double *GRIDVALS);


// errmsg" utilities 
void  tabs_ABORT(int NTAB, char *fileName, char *callFun);
void  missingKey_ABORT(char *key, char *file, char *callFun) ;
void  legacyKey_abort(char *callFun,  char *legacyKey, char *newKey) ;

void  errmsg ( int isev, int iprompt, char *fnam, char *msg1, char *msg2 );
void  prompt(char *msg) ;
void  madend(int flag);    // indicates bad end of program
void  happyend(void) ;    // happy end of program
void  parse_err ( char *infile, int NEWMJD, char *keyword );

void  warn_oldInputs(char *varName_old, char *varName_new);
void  warn_oldinputs__(char *varName_old, char* varName_new) ;

void  checkval_I(char *varname, int nval, 
		 int *iptr, int imin, int imax );
void  checkval_F(char *varname, int nval, 
		 float *fptr, float fmin, float fmax );
void  checkval_D(char *varname, int nval, 
		 double *dptr, double dmin, double dmax );

void  checkval_i__(char *varname, int *nval, 
		   int *iptr, int *imin, int *imax );


void  checkArrayBound(int i, int MIN, int MAX, 
		      char *varName, char *comment, char *funName);
void  checkArrayBound_(int *i, int *MIN, int *MAX, 
		       char *varName, char *comment, char *funName);

void  check_magUndefined(double mag, char *varName, char *callFun );
void  checkStringUnique(char *string, char *msgSource, char *callFun);
int   uniqueMatch(char *string, char *key);
int   uniqueOverlap(char *string, char *key);
int   keyMatch(char *string, char *key);

void read_VARNAMES_KEYS(FILE *fp, int MXVAR, int NVAR_SKIP, char *callFun, 
			int *NVAR, int *NKEY, int *UNIQUE, char **VARNAMES );

unsigned int *CIDMASK_LIST;  int  MXCIDMASK, NCIDMASK_LIST ;
int  exec_cidmask(int mode, int CID);
int  exec_cidmask__(int *mode, int *CID);
void test_cidmask(void) ;

// parameters for errmsg utility 
#define SEV_INFO   1  // severity flag => give info     
#define SEV_WARN   2  // severity flag => give warning  
#define SEV_ERROR  3  // severity flag => error         
#define SEV_FATAL  4  // severity flag => abort program 
#define EXIT_ERRCODE_KCOR           10
#define EXIT_ERRCODE_SIM            11
#define EXIT_ERRCODE_SALT2mu        12
#define EXIT_ERRCODE_combine_fitres 13
#define EXIT_ERRCODE_sntable_dump   14
#define EXIT_ERRCODE_wfit           15
#define EXIT_ERRCODE_merge_root     16
#define EXIT_ERRCODE_merge_hbook    17
#define EXIT_ERRCODE_UNKNOWN        99

// define old useful functions for reading/parsing input file 
void  readint(FILE *fp, int nint, int *list) ;
void  readlong(FILE *fp, int nint, long long *list) ;
void  readfloat(FILE *fp, int nint, float *list);
void  readdouble(FILE *fp, int nint, double *list);
void  readchar(FILE *fp, char *clist) ;

void  read_SURVEYDEF(void);
int   get_IDSURVEY(char *SURVEY);

void  read_redshift(FILE *fp, float *redshift, float *redshift_err );
int   gtchars(char *string, char **argv);
void  debugexit(char *string);

int    INTFILTER ( char *cfilt );  // convert filter name into index


// miscellaneous
				  
double MAGLIMIT_calculator(double ZPT, double PSF, double SKYMAG, double SNR);
double SNR_calculator(double ZPT, double PSF, double SKYMAG, double MAG,
		      double *FLUX_and_ERR ) ;

int getInfo_PHOTOMETRY_VERSION(char *VERSION,  char *DATADIR, 
			       char *LISTFILE, char *READMEFILE);
int getinfo_photometry_version__(char *VERSION,  char *DATADIR, 
				 char *LISTFILE, char *READMEFILE);

int file_timeDif(char *file1, char *file2);

void init_interp_GRIDMAP(int ID, char *MAPNAME, int MAPSIZE, int NDIM, int NFUN,
			 int OPT_EXTRAP, 
			 double **GRIDMAP_INPUT, double **GRIDFUN_INPUT,
			 GRIDMAP *gridmap );

int  interp_GRIDMAP(GRIDMAP *gridmap, double *data, double *interpFun );

void read_GRIDMAP(FILE *fp, char *KEY_ROW, char *KEY_STOP, 
		  int IDMAP, int NDIM, int NFUN, int OPT_EXTRAP, int MXROW,
		  char *callFun, GRIDMAP *GRIDMAP_LOAD );
void warn_NVAR_KEY(char *fileName);

void fillbins(int OPT, char *name, int NBIN, float *RANGE, 
	      float *BINSIZE, float *GRIDVAL);

int commentchar(char *str);
int rd2columnFile(char *file, int MXROW, int *Nrow, 
		   double *column1, double *column2 );
int rd2columnfile_(char *file, int *MXROW, int *Nrow, 
		   double *column1, double *column2 );

int nrow_read(char *file, char *callFun) ;

// -----
double interp_SINFUN(double VAL, double *VALREF, double *FUNREF,
		     char *ABORT_COMMENT ) ;

#define OPT_INTERP_LINEAR    1
#define OPT_INTERP_QUADRATIC 2
double interp_1DFUN(int opt, double val, int NBIN,
		    double *VAL_LIST, double *FUN_LIST, 
		    char *abort_comment );

double interp_1dfun__(int *opt, double *val, int *NBIN,
		      double *VAL_LIST, double *FUN_LIST, 
		      char *abort_comment );

int quickBinSearch(int NBIN, double VAL, double *VAL_LIST, 
		   char *abort_comment);
double quadInterp ( double VAL, double VAL_LIST[3], double FUN_LIST[3],
		    char *abort_comment );

double polyEval(int N, double *coef, double x);

void arrayStat(int N, double *array, double *AVG, double *RMS) ;
double RMSfromSUMS(int N, double SUM, double SQSUM);
void trim_blank_spaces(char *string) ;
void remove_string_termination(char *STRING, int LEN) ;

void splitString(char *string, char *sep, int MXsplit, 
		 int *Nsplit, char **ptrSplit);
void splitString2(char *string, char *sep, int MXsplit,
		  int *Nsplit, char **ptrSplit) ;

void remove_quote(char *string);
void extractStringOpt ( char *string, char *stringOpt) ;
void extract_MODELNAME(char *STRING, char *MODELPATH, char *MODELNAME);
void extract_modelname__(char *STRING, char *MODELPATH, char *MODELNAME);

double PROB_Chi2Ndof(double chi2, int Ndof); // replace CERNLIB's PROB function
double prob_chi2ndof__(double *chi2, int *Ndof);

// pixel-distance to nearest edge
float edgedist(float XPIX, float YPIX,  int NXPIX, int NYPIX);  

void print_banner ( const char *banner ) ;

// shells to open text file
FILE *open_TEXTgz(char *FILENAME, const char *mode,int *GZIPFLAG) ;
FILE *snana_openTextFile (int vboseFlag, char *subdir, char *filename, 
			  char *fullName, int *gzipFlag ); 
void snana_rewind(FILE *fp, char *FILENAME, int GZIPFLAG);

int ENVreplace(char *fileName, char *callFun, int ABORTFLAG);

// cosmology functions

double SFR_integral( double H0, double OM, double OL, double W, double Z );
double SFRfun(double H0, double z) ;
double SFRfun_MD14(double z, double *params);

double dVdz_integral ( double H0, double OM, double OL, double W, 
		       double Zmax, int wgtopt ) ;
double dvdz_integral__ ( double *H0, double *OM, double *OL, double *W, 
			 double *Zmax, int *wgtopt ) ;

double dVdz ( double H0, double OM, double OL, double W, double Z ) ;
double Hzinv_integral ( double H0, double OM, double OL, double W, 
			double Zmin, double Zmax ) ;

double Hainv_integral ( double H0, double OM, double OL, double W, 
			double amin, double amax ) ;

double Hzfun ( double H0, double OM, double OL, double W, double Z ) ;
double dLmag ( double H0, double OM, double OL, double W, 
	       double zCMB, double zHEL ) ;
double dlmag_ (double *H0, double *OM, double *OL, double *W, 
	       double *zCMB, double *zHEL ) ;

double zcmb_dLmag_invert(double H0, double OM, double OL, double W, double MU);

double angSep( double RA1,double DEC1,
	       double RA2,double DEC2, double  scale);


// random-number generators.
// May 2014: snran1 -> Flatran1,  float rangen -> double FlatRan
void   init_RANLIST(void);
double unix_random(void) ;
double FlatRan (int ilist, double *range);  //return rnmd on range[0-1]
double FlatRan1(int ilist);          // return 0 < random  < 1
double GaussRan(int ilist);          // returns gaussian random number
double GaussRanClip(int ilist, double ranGmin, double ranGmax);
int    getRan_Poisson(double mean);
//void   FlatRan_correlated(int NDIM, double *COVRED, double *outRanList);

// mangled functions for fortran
void   randominit_(int *ISEED);  // calls native srandom(ISEED)
double flatran1_(int *ilist) ;          // for fortran
double gaussran_(int *ilist);         // for fortran

// asymmetric gaussians
double biGaussRan(double siglo, double sighi);  // rndm from bivariate guass

double skewGaussRan(double rmin, double rmax, 
		    double siglo, double sighi, double skewlo, double skewhi);

double skewGauss(double x, double siglo,double sighi, 
		 double skewlo, double skewhi) ;

void   init_GaussIntegral(void);
double GaussIntegral(double nsig1, double nsig2);

// SALT2 color law - version 0 from Guy 2007
double SALT2colorlaw0(double lam_rest, double c, double *colorPar );

// SALT2 color law - version 1 from Guy 2010
double SALT2colorlaw1(double lam_rest, double c, double *colorPar );

double SALT2colorfun_dpol(const double rl, int nparams, 
			  const double *params, const double alpha);
double SALT2colorfun_pol(const double rl, int nparams, 
			 const double *params, const double alpha);


// ------ index mapping
void clear_1DINDEX(int ID);
void  init_1DINDEX(int ID, int NDIM, int *NPT_PERDIM );
int    get_1DINDEX(int ID, int NDIM, int *indx );

// mangled functions for fortran
void clear_1dindex__(int *ID);
void init_1dindex__(int *ID, int *NDIM, int *NPT_PERDIM );
int  get_1dindex__ (int *ID, int *NDIM, int *indx );
void set_sndata__(char *key, int *NVAL, char *stringVal, double *parVal ) ;

// ------ sorting --------

void sortDouble(int NSORT, double *ARRAY, int ORDER, 
		int *INDEX_SORT) ;
void sortFloat(int NSORT, float *ARRAY, int ORDER, 
	       int *INDEX_SORT) ;
void sortInt(int NSORT, int *ARRAY, int ORDER, 
	     int *INDEX_SORT) ;
void sortLong(int NSORT, long long *ARRAY, int ORDER, 
	     int *INDEX_SORT) ;
void reverse_INDEX_SORT(int NSORT, int *INDEX_SORT) ;


// mangled fortran functions
void sortdouble_(int *NSORT, double *ARRAY, int *ORDER, 
		 int *INDEX_SORT);
void sortfloat_(int *NSORT, float *ARRAY, int *ORDER, 
		int *INDEX_SORT);
void sortint_(int *NSORT, int *ARRAY, int *ORDER, 
	      int *INDEX_SORT);

// invert matrix to replace CERNLIB functions
void invertMatrix (int  N, int  n, double *Matrix ) ;
void invertmatrix_(int *N, int *n, double *Matrix ) ;

double zhelio_zcmb_translator (double z_input, double RA, double DECL, 
			       char *coordSys, int OPT ) ;
double zhelio_zcmb_translator__ (double *z_input, double *RA, double *DECL, 
				 char *coordSys, int *OPT ) ;
double Z2CMB (double z_helio, double RA, double DECL, char *coordSys ) ;
double z2cmb_(double *z_helio, double *RA, double *DECL, char *coordSys );

// SLALIB functions translated by D. Cinabro
void slaEqgal ( double dr, double dd, double *dl, double *db );
void slaDcs2c ( double a, double b, double v[3] );
void slaDmxv ( double dm[3][3], double va[3], double vb[3] );
void slaDcc2s ( double v[3], double *a, double *b );
double slaDrange ( double angle );
double slaDranrm ( double angle );

// functions for user-define PATH_SNDATA_SIM
#define MXPATH_SNDATA_SIM 20
void add_PATH_SNDATA_SIM(char *PATH);
int  getList_PATH_SNDATA_SIM(char **pathList);
FILE *openFile_PATH_SNDATA_SIM(char *mode);

// generic LC width util (for simulation)
void   init_lightCurveWidth(void);
double get_lightCurveWidth(int OPTMASK, int NOBS, double *TLIST, 
			   double *MAGLIST, int *ERRFLAG, char *FUNCALL ) ;

void   init_lightcurvewidth__(void);
double get_lightcurvewidth__(int *OPTMASK, int *NOBS, double *TLIST, 
			   double *MAGLIST, int *ERRFLAG, char *FUNCALL ) ;

// ============== END OF FILE =============