/*******************************************************************
**                                                                **
**  Knihovna pro kontrolu prace s alokovanou pameti a se soubory  **
**  Autor: Pavel Sabatka a Petr Petyovsky                         **
**  Implementovano v ramci bakalarske prace                       **
**  VUT v Brne, UAMT, FEKT v ak. roce 2007, 2008, 2010 - 2019     **
**  rel. 2012.25                                                  **
**                                                                **
*******************************************************************/
/* $Id: check.cpp 3 2022-10-26 17:03:14Z petyovsky $ */

#include "check.h"

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef fopen
#undef fclose

#ifdef __cplusplus
	#undef new
	#undef delete
#endif /* __cplusplus */

/* Boolean type support */
#ifdef __cplusplus					/* C++ bool type support */
	#define CHECKER_BOOL	bool
	#define CHECKER_TRUE	true
	#define CHECKER_FALSE	false
	#define CHECKER_EMPTY_INIT_LIST {}

#elif (__STDC_VERSION__ >= 199901L) 			/* C99 bool type support */
	#include <stdbool.h>
	#define CHECKER_BOOL	bool
	#define CHECKER_TRUE	true
	#define CHECKER_FALSE	false
	#define CHECKER_EMPTY_INIT_LIST {0,}

#else							/* C90 bool type replacement */
	#define CHECKER_BOOL	int
	#define CHECKER_TRUE	1
	#define CHECKER_FALSE	0
	#define CHECKER_EMPTY_INIT_LIST {0,}
#endif /*__cplusplus */

/* nullptr C++11 keyword support */
#if defined(__cplusplus) && ( (__cplusplus >= 201103L) || (_MSC_VER >= 1600) ) /* C++11 or higher, Visual Studio 2010 or higher */
	#define CHECKER_NULL	nullptr
#else
	#define CHECKER_NULL	NULL
#endif /*__cplusplus */

/* Libc size_t support in the printf format */
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
	#define CHECKER_SIZE_T_PRIu		"%zu"	/* pro typ size_t v C99 a C++11 jiz definovan modifikator velikosti (%z) */
	#define CHECKER_SIZE_T_PRId		"%zd"
	#define CHECKER_SIZE_T_CONV		size_t
#elif (_MSC_VER >= 1310)	
	#define CHECKER_SIZE_T_PRIu		"%Iu"	/* od VS2003 definovan modifikator velikosti (%Iu) pro typ size_t, ktery je bud I32 nebo I64 */
	#define CHECKER_SIZE_T_PRId		"%Id"
	#define CHECKER_SIZE_T_CONV		size_t
#else
	#define CHECKER_SIZE_T_PRIu		"%lu"	/* retro compilers */
	#define CHECKER_SIZE_T_PRId		"%ld"
	#define CHECKER_SIZE_T_CONV		unsigned long int
#endif /* __STDC_VERSION__ */

/* Compiler and libraries support of the C11 Security Annex K (fopen_s, strncpy_s, strnlen_s, fprintf_s, ...) */
#if ((__STDC_LIB_EXT1__ >= 201112L) && (__STDC_WANT_LIB_EXT1__ != 0)) || (_MSC_VER >= 1600) /* C11 Security Annex K or higher, Visual Studio 2010 or higher */
	#define CHECKER_SECURE_SUPPORT
	#define CHECKER_STRNCPY_S(aDest, aDestsz, aSrc, aCount)		strncpy_s((aDest), (aDestsz), (aSrc), (aCount))
	#define CHECKER_STRNLEN_S(aStr, aDestsz)					strnlen_s((aStr), (aDestsz))  
/* Problem s PATH_MAX a FILENAME_MAX, proto na nekterych mistech ponechan strlen */
/* FILENAME_MAXSIZE: <stdio.h> define FILENAME_MAX,   linux, unix, windows compatible */

#else
	#undef  CHECKER_SECURE_SUPPORT
	#define CHECKER_STRNCPY_S(aDest, aDestsz, aSrc, aCount)		strncpy((aDest), (aSrc), (aCount))
	#define CHECKER_STRNLEN_S(aStr, aDestsz)					strlen(aStr)
#endif /* __STDC_VERSION__ */

/* A macro for C/C++ compatibility of the unused parameter in the function body. 
   C++ say: If a parameter is not used in the function body, it does not need to be named in the function prototype. 
   otherwise C compilers emit error: expected formal parameter list, not a type list 
*/
#ifdef __cplusplus
	#define UNUSED_PARAM(aPar)
#else
	#define UNUSED_PARAM(aPar) aPar
#endif

/* include knihoven */
#include <string.h>
#include <time.h>

#define HIDDEN_FUNC_NAME "##"
#define HIDDEN_SOURCE_FILE_NAME "#HiddenSource#"

#define PAGE_WIDTH 79

/****************************
*      DATOVE TYPY          *
****************************/
enum TAllocType {EMalloc = 0, ECalloc, ERealloc, ENew, ENewA, ENewNothrow, ENewNothrowA, ECount};
static const char *alloc_type_str[ECount] = {"malloc", "calloc", "realloc", "new", "new[]", "new(nothrow)", "new(nothrow)[]"};
#define CHECKER_MAXLENGTH_ALLOC_TYPE_STR 15		/* sizeof("new(nothrow)[]") = 15 */

/* prvek linearniho seznamu alokovane pameti */
struct TNodeMem 
	{
	struct TNodeMem *ptr;		// ukazatel na dalsi prvek
	void *memory_address;		// adresa alokovane pameti
	size_t size;				// alokovana velikost
	unsigned int linenum;		// radek zdrojoveho kodu (makro __LINE__)
	unsigned int alloc_num;		// absolutni poradi alokace
	char file[__CHECKER_MAXLENGTH_FILENAME__];
	char func[__CHECKER_MAXLENGTH_FUNCNAME__];
	enum TAllocType type_func;
	};

/* linearni seznam alokovane pameti */
struct TListMem
	{
	struct TNodeMem *act;
	struct TNodeMem *first;

	// statistiky programu
	size_t size_alloc_max_peak;
	size_t size_alloc;
	size_t size_free;
	unsigned int count_alloc;
	unsigned int count_realloc;
	unsigned int count_dealloc;

	size_t count_alloc_lock_key;
	unsigned int count_alloc_lock_limit;
	size_t count_realloc_lock_key;
	unsigned int count_realloc_lock_limit;
	};

/* prvek linearniho seznamu otevrenych souboru */
struct TNodeFile 
	{
	struct TNodeFile *ptr;		// ukazatel na dalsi prvek
	FILE *memory_address;		// adresa handleru FILE
	unsigned int linenum;		// radek zdrojoveho kodu (makro __LINE__)
	char file[__CHECKER_MAXLENGTH_FILENAME__];		// soubor, ve kterem je volano fopen
	char func[__CHECKER_MAXLENGTH_FUNCNAME__];	// funkce, ve ktere je volano fopen
	char path[__CHECKER_MAXLENGTH_PATHNAME__];		// jmeno souboru
	char mode[__CHECKER_MAXLENGTH_FILEMODE__];	// parametry otevirani souboru
	};

/* linearni seznam otevrenych souboru */
struct TListFile 
	{
	struct TNodeFile *act;
	struct TNodeFile *first;

	// statistiky programu
	unsigned int count_fopen;
	unsigned int count_fclose;
	};

/****************************
*          FUNKCE           *
****************************/
//  prace se seznamem - funkce pro kontrolu alokace v C
static void allocc_init_list(struct TListMem *aLm);					// inicializace
static void allocc_insert(struct TListMem*, size_t, void*, unsigned int, const char[], const char [], enum TAllocType);		// vlozeni prvku
static void allocc_resize_act(struct TListMem*, size_t, void*, unsigned int, const char[], const char [], enum TAllocType);	// zmena parametru pouzite pro realloc()
static void allocc_dispose_act(struct TListMem *L);					// zrusi aktualni prvek
static void allocc_set_act(struct TListMem *L, void *address);		// nastavi prvek se zadanou adresou jako aktualni
static void allocc_set_last_act(struct TListMem *L);				// nastavi posledni prvek jako aktualni
static CHECKER_BOOL allocc_active(const struct TListMem *L);		// overi, jestli je seznam aktivni

// prace se seznamem - funkce pro kontrolu souboru v C
static void filec_init_list(struct TListFile *LS);				// inicializace
static void filec_insert(struct TListFile *, const char [], const char [], unsigned int, const char [], const char [], FILE *); // vlozeni prvku
static void filec_dispose_act(struct TListFile *L);				// zrusi aktualni prvek
static void filec_set_act(struct TListFile *L, FILE *adress);	// nastavi prvek se zadanou adresou jako aktualni
static void filec_set_last_act(struct TListFile *L);			// nastavi posledni prvek jako aktualni
static CHECKER_BOOL filec_active(const struct TListFile *L);	// overi, jestli je seznam aktivni

// prace se seznamem - pro oba seznamy
static void dispose_lists (struct TListMem*, struct TListFile *);		// zruseni vsech seznamu

// dalsi funkce
static void checkdel(void);								// tuto funkci pridavam do funkce exit
static void program_error(void);						// vyvola chybu, korektne uvolni pamet a soubory, ukonci program
static void print_lists(struct TListMem *aL, struct TListFile *aLF, CHECKER_BOOL aPrint_alloc, CHECKER_BOOL aPrint_file); // vypise statistiky programu
static void printchar(size_t from, size_t to, char ch); // opakovane vypisuje znak ch dokud from != to
static unsigned int chars_in_dec(size_t val);			// Vraci pocet znaku nutnych k vytisknuti cisla v desitkove soustave
static unsigned int chars_in_hex(ptrdiff_t val);		// Vraci pocet znaku nutnych k vytisknuti cisla v sestnactkove soustave

/***************************************
*  IMPLEMENTACE STATICKYCH PROMENNYCH  *
****************************************/
static struct TListMem L = CHECKER_EMPTY_INIT_LIST;		// seznam alokovane pameti
static struct TListFile LF = CHECKER_EMPTY_INIT_LIST;	// seznam otevrenych souboru

static CHECKER_BOOL used_alloc =  CHECKER_FALSE;	// bool - byla jiz alokovana nejaka pamet?
static CHECKER_BOOL used_file_c = CHECKER_FALSE;	// bool - byl jiz otevren nejaky soubor?
static CHECKER_BOOL correct_exit = CHECKER_TRUE;	// bool - byl program ukoncen bez chyby

#ifdef __cplusplus
	// Mailbox pro docasne ulozeni adresy dealokovane pameti pri volani operatoru delete
	static void *chk_delete_err_ptr = CHECKER_NULL; 
#endif /* __cplusplus */

#ifdef __cplusplus
	// Mailbox pro docasne ulozeni _LINE_ _FILE_ _FUNC_ hodnot pri volani operatoru new
	static CHECKER_BOOL chk_new_record_full = CHECKER_FALSE;  // bool - Je ulozen v temp delete mailboxu nejaky zaznam?
	static unsigned int chk_new_line = 0;
	static char chk_new_soubor[__CHECKER_MAXLENGTH_FILENAME__] = {'\0'};
	static char chk_new_func[__CHECKER_MAXLENGTH_FUNCNAME__] = {'\0'};
#endif /* __cplusplus */

/********************************************
*  IMPLEMENTACE FUNKCI  - UZIVATELSKYCH     *
********************************************/
/* vypise alokovanou pamet a otevrene soubory */
void chk_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	fprintf(CHECKER_OUT, "%s: %u :%s\n" ,soubor, line, func);
	print_lists(&L, &LF, CHECKER_TRUE, CHECKER_TRUE);
	}

/* vypise aktualni stav alokovane pameti */
void chk_memory_stat (unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	fprintf(CHECKER_OUT, "%s: %u :%s\n" ,soubor, line, func);
	print_lists(&L, &LF, CHECKER_TRUE, CHECKER_FALSE);
	}

/* vypise aktualni otevrene soubory */
void chk_file_stat (unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	fprintf(CHECKER_OUT, "%s: %u: %s\n", soubor, line, func);
	print_lists(&L, &LF, CHECKER_FALSE, CHECKER_TRUE);
	}

/* vraci nejvyssi vrchol alokovane pameti (od spusteni programu az po soucasnost) */
size_t chk_get_alloc_max_peak_size(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.size_alloc_max_peak;
	}

/* vraci sumu celkove naalokovane pameti */
size_t chk_get_alloc_total_size(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.size_alloc;
	}

/* vraci sumu celkove dealokovane pameti */
size_t chk_get_dealloc_total_size(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.size_free;
	}

/* vraci velikost aktualne alokovane pameti */
size_t chk_get_alloc_actual_size(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.size_alloc - L.size_free;
	}

/* vraci celkovy pocet pozadavku na alokaci pameti */
unsigned int chk_get_alloc_count(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.count_alloc;
	}

/* vraci celkovy pocet pozadavku na realokaci pameti */
unsigned int chk_get_realloc_count(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.count_realloc;
	}

/* vraci celkovy pocet pozadavku na dealokaci pameti */
unsigned int chk_get_dealloc_count(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return L.count_dealloc;
	}

/* vraci celkovy pocet pozadavku na otevreni souboru */
unsigned int chk_get_fopen_count(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return LF.count_fopen;
	}

/* vraci celkovy pocet pozadavku na zavreni souboru */
unsigned int chk_get_fclose_count(unsigned int UNUSED_PARAM(line), const char UNUSED_PARAM(soubor)[__CHECKER_MAXLENGTH_FILENAME__], const char UNUSED_PARAM(func)[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	return LF.count_fclose;
	}

/* Aktivace zamku alokaci, vsechny alokace po poctu vetsim nez aAfterLimit budou hlasit chybu alokace(vcetne volani funkce realloc ktere funguji jako malloc alokace), funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
size_t chk_lock_alloc(unsigned int aAfterLimit, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	if(L.count_alloc_lock_key)
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nAlokace jiz byla zamknuta!!!\n"
			"Zdrojovy soubor:radek: %s:%u\n"
			"Volani funkce: %s()\n", soubor, line, func);
		return 0;
		}
	else
		{
		srand((unsigned)time(NULL));
		L.count_alloc_lock_key = (size_t) malloc(10);
		free((void*) L.count_alloc_lock_key);
		L.count_alloc_lock_key ^= rand();
		L.count_alloc_lock_limit = L.count_alloc + aAfterLimit;
		}
	return L.count_alloc_lock_key;
	}

/* Odemknuti zamku alokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
size_t chk_unlock_alloc(size_t aAlloc_lock_key, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	if(L.count_alloc_lock_key != aAlloc_lock_key)
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nPokus o odemknuti zamku alokace spatnym klicem!!!\n"
			"Zdrojovy soubor:radek: %s:%u\n"
			"Volani funkce: %s()\n", soubor, line, func);
		return 0;
		}
	L.count_alloc_lock_key = 0;
	L.count_alloc_lock_limit = 0;
	return 1;
	}

/* Aktivace zamku realokaci, vsechny realokace po poctu vetsim nez aAfterLimit budou hlasit chybu realokace, (mimo volani funkce realloc ktery funguji jako free nebo jako malloc), funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
size_t chk_lock_realloc(unsigned int aAfterLimit, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	if(L.count_realloc_lock_key)
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nRealokace jiz byla zamknuta!!!\n"
			"Zdrojovy soubor:radek: %s:%u\n"
			"Volani funkce: %s()\n", soubor, line, func);
		return 0;
		}
	else
		{
		srand((unsigned)time(NULL));
		L.count_realloc_lock_key = (size_t)malloc(10);
		free((void*)L.count_realloc_lock_key);
		L.count_realloc_lock_key ^= rand();
		L.count_realloc_lock_limit = L.count_realloc + aAfterLimit;
		}
	return L.count_realloc_lock_key;
	}

/* Odemknuti zamku realokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
size_t chk_unlock_realloc(size_t aRealloc_lock_key, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	if(L.count_realloc_lock_key != aRealloc_lock_key)
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nPokus o odemknuti zamku realokace spatnym klicem!!!\n"
			"Zdrojovy soubor:radek: %s:%u\n"
			"Volani funkce: %s()\n", soubor, line, func);
		return 0;
		}
	L.count_realloc_lock_key = 0;
	L.count_realloc_lock_limit = 0;
	return 1;
	}

/***************************************************
*  IMPLEMENTACE ALOKACNICH A DEALOKACNICH FUNKCI - C *
***************************************************/
void* chk_malloc(size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *novyPtr = CHECKER_NULL;
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		
		if (used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	
	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		novyPtr = (void *) malloc(size);
	
	if (novyPtr) 
		allocc_insert(&L, size, novyPtr, line, soubor, func, EMalloc);
	else
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n"
								"---------------------------------\n"
								"Chyba pri volani funkce void* malloc(size_t)\n"
				);
	return novyPtr;
	}

void* chk_calloc(size_t count, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *novyPtr = CHECKER_NULL;
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if (used_file_c == CHECKER_FALSE)
			atexit(&checkdel);
		}
	
	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		novyPtr = (void *) calloc(count, size);
	
	if (novyPtr) 
		allocc_insert (&L, size*count, novyPtr, line, soubor, func, ECalloc);
	else
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n"
								"------------------------------\n"
								"Chyba pri volani funkce void* calloc(size_t, size_t)\n"
				);

	return novyPtr;
	}

void* chk_realloc(void* realloc_ptr, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	void *newPtr = CHECKER_NULL;

	if(size == 0) 		// tzn. jiny zapis funkce free
		{
		allocc_set_act(&L, realloc_ptr);

		if (allocc_active(&L)) 		// uvolnuji korektne pridelenou pamet
			{
#ifdef __cplusplus
			if( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA) )
				{
				fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci realloc() kompletne uvolnujete pamet: %p, ktera byla alokovana operatorem %s(). Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena.\n\n", realloc_ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
				fprintf(CHECKER_OUT, "------------------------------\n"
						"Lokace varovani (odalokace adresy: %p):\n"
						"Zdrojovy soubor:radek: %s:%u\n", realloc_ptr, soubor, line);
				fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
				fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
				return(realloc_ptr);
				}
#endif
			allocc_dispose_act(&L);
			free(realloc_ptr);
			}
		else					// uvolnuji pamet, ktera mi nebyla pridelena
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n");
			fprintf(CHECKER_OUT, "Volanim funkce\nvoid realloc(void*, size_t)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", realloc_ptr);
			fprintf(CHECKER_OUT, "\n-------------------------------------\n"
					"Lokace chyby:\n"
					"Zdrojovy soubor:radek: %s:%u\n", soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			program_error();
			}
		}  // size == 0
	else				// budeme pracovat s pameti (alokovat / realokovat)
		{
		if(realloc_ptr == CHECKER_NULL) // tj. jiny zapis funkce malloc
			{
			if (used_alloc == CHECKER_FALSE) 	// jestlize jeste nebylo alokovano
				{
				used_alloc = CHECKER_TRUE;
				allocc_init_list(&L);
				if (used_file_c == CHECKER_FALSE) 
					atexit(&checkdel);
				}
			
			if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
				newPtr = (void *) malloc(size);
			
			if(newPtr)
				allocc_insert (&L, size, newPtr, line, soubor, func, ERealloc);
			else
				fprintf(CHECKER_OUT, "CHECKER: Pri alokaci pameti nastala chyba !\n------------------------------\nChyba pri volani funkce void* realloc(void *, size_t,)");
			
			} // if (realloc_ptr == CHECKER_NULL)
		else				// volani funkce realloc
			{
			allocc_set_act (&L, realloc_ptr);
#ifdef __cplusplus
			if( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA) )
				{
				fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci realloc() menite velikost pameti: %p, ktera byla alokovana operatorem %s(). Norma jazyka C++ tuto operaci nepovoluje. Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena ani zmenena jeji velikost.\n\n", realloc_ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
				fprintf(CHECKER_OUT, "------------------------------\n"
						"Lokace varovani (realokace adresy: %p):\n"
						"Zdrojovy soubor:radek: %s:%u\n", realloc_ptr, soubor, line);
				fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
				fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
				return(realloc_ptr);
				}
#endif
			if(!allocc_active(&L))
				{
				fprintf(CHECKER_OUT, "\n\nCHECKER: Chyba - prvni parametr funkce\n void* realloc (void*, size_t) \nzadavate ukazatel na misto v pameti, ktere vam nebylo prideleno ! !\n-----------------------------------------------------\n");
				return(CHECKER_NULL);
				}
			
			if((!L.count_realloc_lock_key) || (L.count_realloc < L.count_realloc_lock_limit))
				newPtr = (void *) realloc(realloc_ptr, size);
			
			if (newPtr) 
				allocc_resize_act(&L, size, newPtr, line, soubor, func, ERealloc);
			else
				fprintf(CHECKER_OUT, "CHECKER: Pri alokaci pameti nastala chyba !\n------------------------------\nChyba pri volani funkce void* realloc(void *, size_t,)");
			
			} // else- if (realloc_ptr != CHECKER_NULL)
		return newPtr;
		} // else- if (size > 0)
	return CHECKER_NULL;
	}

void chk_free(void *Ptr, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char name_fce[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	allocc_set_act(&L, Ptr);

	if (allocc_active(&L)) 
		{
		if ( (L.act->type_func == ENew) || (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrow) || (L.act->type_func == ENewNothrowA)) 
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nFunkci free() uvolnujete pamet: %p, ktera byla alokovana operatorem %s(). Spravne by mela byt uvolnena operatorem delete%s(). Pozor, pamet nebyla uvolnena.\n\n", Ptr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA) )? ("[]") : (""));
			fprintf(CHECKER_OUT, "------------------------------\n"
								  "Lokace varovani (odalokace adresy: %p):\n"
								  "Zdrojovy soubor:radek: %s:%u\n", Ptr, soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", name_fce);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			}
		else
			{
			allocc_dispose_act(&L);
			free(Ptr);
			}
		}
	else
		if(Ptr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
								 "Volanim funkce\nvoid free(void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", Ptr);
		
			fprintf(CHECKER_OUT, "------------------------------\n"
								 "Lokace chyby:\n"
								  "Zdrojovy soubor:radek: %s:%u\n", soubor, line);
			fprintf(CHECKER_OUT, "Volani funkce: %s()\n", name_fce);
			fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
			program_error();
			}
	}


/*******************************************************
*  IMPLEMENTACE ALOKACNI / DEALOKACNI OPERATORU - C++  *
*******************************************************/
#ifdef __cplusplus

bool chk_new_recorder(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	size_t len = 0;
	if(chk_new_record_full == CHECKER_TRUE)
		fprintf(CHECKER_OUT, "New record mailbox isn't empty:\nnew(%s:%u:%s)\n<-\nnew(%s:%u:%s)\n", 
				chk_new_soubor, chk_new_line, chk_new_func, soubor, line, func);
	chk_new_line = line;
	len = strlen(soubor) + 1;
	if(len > __CHECKER_MAXLENGTH_FILENAME__ )
		CHECKER_STRNCPY_S(chk_new_soubor, __CHECKER_MAXLENGTH_FILENAME__, soubor + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
	else
		CHECKER_STRNCPY_S(chk_new_soubor, __CHECKER_MAXLENGTH_FILENAME__, soubor, __CHECKER_MAXLENGTH_FILENAME__);
	
	CHECKER_STRNCPY_S(chk_new_func, __CHECKER_MAXLENGTH_FUNCNAME__, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);
	chk_new_func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';
	chk_new_record_full = CHECKER_TRUE;
	return(false);
	}

void* operator new(size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION
	{
	void *addr = CHECKER_NULL;
	
	if (used_alloc == CHECKER_FALSE)
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE)
			atexit(&checkdel);
		}
	
	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		addr = (void *)malloc(aSize);
	
	if(addr)
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENew);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENew);
		}
	else
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new(size_t)");
		throw std::bad_alloc();
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}

void* operator new(size_t aSize, const std::nothrow_t& UNUSED_PARAM(aNoThrow)) _CHECKER_NO_EXCEPTION
	{
	void *addr = CHECKER_NULL;
	
	if(used_alloc == CHECKER_FALSE)
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if (used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}

	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		addr = (void *) malloc(aSize);
	
	if(addr)
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewNothrow);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewNothrow);
		}
	else
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new(size_t,nothrow)");
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}

void* operator new[](size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION
	{
	void *addr = CHECKER_NULL;
	
	if(used_alloc == CHECKER_FALSE)
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	
	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		addr = (void *)malloc(aSize);
	
	if(addr)
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewA);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewA);
		}
	else
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new[](size_t)");
		throw std::bad_alloc();
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}

void* operator new[](size_t aSize, const std::nothrow_t& UNUSED_PARAM(aNoThrow)) _CHECKER_NO_EXCEPTION
	{
	void *addr = CHECKER_NULL;
	
	if (used_alloc == CHECKER_FALSE) 
		{
		used_alloc = CHECKER_TRUE;
		allocc_init_list(&L);
		if(used_file_c == CHECKER_FALSE) 
			atexit(&checkdel);
		}
	
	if((!L.count_alloc_lock_key) || (L.count_alloc < L.count_alloc_lock_limit))
		addr = (void *) malloc(aSize);
	
	if(addr)
		{
		if(chk_new_record_full)
			allocc_insert (&L, aSize, addr, chk_new_line, chk_new_soubor, chk_new_func, ENewNothrowA);
		else
			allocc_insert (&L, aSize, addr, 0, HIDDEN_SOURCE_FILE_NAME, HIDDEN_FUNC_NAME, ENewNothrowA);
		}
	else
		{
		fprintf(CHECKER_OUT, "Pri alokaci pameti nastala chyba !\n---------------------------------\nChyba pri alokaci pomoci operatoru void* operator new[](size_t,nothrow)");
		}
	chk_new_record_full = CHECKER_FALSE;
	return(addr);
	}

CChkDeleteRecorder::~CChkDeleteRecorder()
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
			"Lokace chyby (odalokace adresy: %p):\n"
			"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, iFile, iLine);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", iFunc);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;
	}

void operator delete(void *aPtr) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;

	allocc_set_act(&L, aPtr);

	if(allocc_active(&L))
		{
		if( (L.act->type_func != ENew) && (L.act->type_func != ENewNothrow) )
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem delete() uvolnujete pamet: %p, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\nPozor, pamet nebyla uvolnena.\n", 
				aPtr, alloc_type_str[L.act->type_func], ((L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA)) ? ("delete[]") : ("free") );
			chk_delete_err_ptr = aPtr;
			}
		else
			{
			allocc_dispose_act(&L);
			free(aPtr);
			}
		}
	else
		if(aPtr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
					"Volanim operatoru \ndelete(void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", aPtr);
			chk_delete_err_ptr = aPtr;
			program_error();
			}
	}

void operator delete[](void *aPtr) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;
	
	allocc_set_act(&L, aPtr);

	if (allocc_active(&L))
		{
		if( (L.act->type_func != ENewA) && (L.act->type_func != ENewNothrowA) )
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem delete[]() uvolnujete pamet: %p, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\n",
				aPtr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENew) || (L.act->type_func == ENewNothrow)) ? ("delete") : ("free") );
			fprintf(CHECKER_OUT, "Alokace pameti byla puvodne provedena pomoci: %s() v souboru: %s:%u, ve funkci: %s().\nPozor, pamet nebyla uvolnena.\n",
				alloc_type_str[L.act->type_func], L.act->file, L.act->linenum, L.act->func);
			chk_delete_err_ptr = aPtr;
			}
		else
			{
			allocc_dispose_act(&L);
			free(aPtr);
			}
		}
	else
		if(aPtr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
					"Volanim operatoru \ndelete[](void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", aPtr);
			chk_delete_err_ptr = aPtr;
			program_error();
			}
	}

#if (__cplusplus >= 201402L) || (_MSC_FULL_VER >= 190023918L)		/* C++14 global sized dealocation feature */

void operator delete(void *aPtr, size_t aSize) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
			"Lokace chyby (odalokace adresy: %p) (velikosti: " CHECKER_SIZE_T_PRIu " :\n"
			"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, (CHECKER_SIZE_T_CONV)aSize, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;

	allocc_set_act(&L, aPtr);

	if(allocc_active(&L))
		{
		CHECKER_BOOL is_warning = CHECKER_FALSE;
		if((L.act->type_func != ENew) && (L.act->type_func != ENewNothrow))
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem sized delete() uvolnujete na adrese: %p pamet, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\n",
				aPtr, alloc_type_str[L.act->type_func], ((L.act->type_func == ENewA) || (L.act->type_func == ENewNothrowA)) ? ("delete[]") : ("free"));
			chk_delete_err_ptr = aPtr;
			is_warning = CHECKER_TRUE;
			}
		if(L.act->size != aSize)
			{
			if(!is_warning)
				fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\n");
			fprintf(CHECKER_OUT, "Operatorem sized delete() uvolnujete na adrese: %p, blok pameti o velikosti: " CHECKER_SIZE_T_PRIu ". Ovsem puvodni alokovana velikost bloku byla: " CHECKER_SIZE_T_PRIu "!\n",
				aPtr, (CHECKER_SIZE_T_CONV)aSize, (CHECKER_SIZE_T_CONV)L.act->size);
			chk_delete_err_ptr = aPtr;
			is_warning = CHECKER_TRUE;
			}
		if(is_warning)
			fprintf(CHECKER_OUT, "Alokace pameti byla puvodne provedena pomoci: %s() v souboru: %s:%u, ve funkci: %s().\n" "Pozor, pamet nebyla uvolnena.\n",
				alloc_type_str[L.act->type_func], L.act->file, L.act->linenum, L.act->func);
		else
			{
			allocc_dispose_act(&L);
			free(aPtr);
			}
		}
	else
		if(aPtr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
				"Volanim operatoru\nsized delete(void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", aPtr);
			chk_delete_err_ptr = aPtr;
			program_error();
			}
	}

void operator delete[](void *aPtr, size_t aSize) _CHECKER_NO_EXCEPTION
	{
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p) (velikosti: " CHECKER_SIZE_T_PRIu " :\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, (CHECKER_SIZE_T_CONV)aSize, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;
	
	allocc_set_act(&L, aPtr);

	if (allocc_active(&L))
		{
		CHECKER_BOOL is_warning = CHECKER_FALSE;
		if( (L.act->type_func != ENewA) && (L.act->type_func != ENewNothrowA) )
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\nOperatorem sized delete[]() uvolnujete na adrese: %p pamet, ktera byla alokovana pomoci %s(). Spravne by mela byt uvolnena pomoci %s().\n",
				aPtr, alloc_type_str[L.act->type_func], ( (L.act->type_func == ENew) || (L.act->type_func == ENewNothrow)) ? ("delete") : ("free") );
			chk_delete_err_ptr = aPtr;
			is_warning = CHECKER_TRUE;
			}
		if(L.act->size != aSize)
			{
			if(!is_warning)
				fprintf(CHECKER_OUT, "\n\nCHECKER: VAROVANI\n");
			fprintf(CHECKER_OUT, "Operatorem sized delete[]() uvolnujete na adrese: %p, blok pameti o velikosti: " CHECKER_SIZE_T_PRIu ". Ovsem puvodni alokovana velikost bloku byla: " CHECKER_SIZE_T_PRIu "!\n",
				aPtr, (CHECKER_SIZE_T_CONV)aSize, (CHECKER_SIZE_T_CONV)L.act->size);
			if(!is_warning)
				fprintf(CHECKER_OUT, "Alokace pameti byla puvodne provedena pomoci: %s() v souboru: %s:%u, ve funkci: %s().\n",
					alloc_type_str[L.act->type_func], L.act->file, L.act->linenum, L.act->func);
			chk_delete_err_ptr = aPtr;
			is_warning = CHECKER_TRUE;
			}
		if(is_warning)
			fprintf(CHECKER_OUT, "Alokace pameti byla puvodne provedena pomoci: %s() v souboru: %s:%u, ve funkci: %s().\n" "Pozor, pamet nebyla uvolnena.\n",
				alloc_type_str[L.act->type_func], L.act->file, L.act->linenum, L.act->func);
		else
			{
			allocc_dispose_act(&L);
			free(aPtr);
			}
		}
	else
		if(aPtr)
			{
			fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
					"Volanim operatoru \nsized delete[](void*)\nse pokousite uvolnit pamet na adrese: %p, ktera Vam nebyla pridelena.\n", aPtr);
			chk_delete_err_ptr = aPtr;
			program_error();
			}
	}

#endif /* C++14 */

#endif /* __cplusplus */

/***************************************
*  IMPLEMENTACE SOUBOROVYCH FUNKCI - C *
****************************************/
FILE* chk_fopen(const char file[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char filesource[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]) 
	{
	FILE *novyPtr = CHECKER_NULL;
	if (used_file_c == CHECKER_FALSE) 
		{
		used_file_c = CHECKER_TRUE;
		filec_init_list(&LF);
		if (used_alloc == CHECKER_FALSE) atexit(&checkdel);
		}

	if((file == CHECKER_NULL) || (mode == NULL))
		{
		fprintf(CHECKER_OUT, "CHECKER: CHYBA\n------------------------------\n"
							 "Pri volanim funkce: fopen() vznikly tyto problemy:\n");
		if(file == CHECKER_NULL)
			fprintf(CHECKER_OUT, "Ukazatel na nazev souboru obsahuje NULL !\n");
		if(mode == CHECKER_NULL)
			fprintf(CHECKER_OUT, "Ukazatel na mod otevreni souboru obsahuje NULL !\n");
		fprintf(CHECKER_OUT, "Soubor nemohl byt otevren\n\n");
		printchar(0, PAGE_WIDTH, '-');
		fprintf(CHECKER_OUT, "\nLokace chyby:\n"
							 "Zdrojovy soubor:radek: %s:%u\n", filesource, line);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
		program_error();
		}

#ifdef CHECKER_SECURE_SUPPORT
	/* When using fopen_s or freopen_s, file access permissions for any file created with "w" or "a" prevents other users from accessing it. 
	 * File access mode flag "u" can optionally be prepended to any specifier that begins with "w" or "a", to enable the default fopen permissions. (C11) */
	errno_t err = fopen_s(&novyPtr, file, mode);
	if((!err) && (novyPtr != CHECKER_NULL))
#else
	if((novyPtr = fopen(file, mode)) != CHECKER_NULL)
#endif
		{
		filec_insert(&LF, file, mode, line, filesource, func, novyPtr);
		LF.count_fopen++;
		}
	else
		{
		fprintf(CHECKER_OUT, "CHECKER: VAROVANI\n------------------------------\n"
							 "Soubor %s se nepodarilo otevrit\n\n", file);
		printchar(0, PAGE_WIDTH, '-');
		fprintf(CHECKER_OUT, "\nLokace varovani:\n"
							 "Zdrojovy soubor:radek: %s:%u\n", filesource, line);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
		}
	return novyPtr;
	}

int chk_fclose(FILE* streamPtr, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__])
	{
	int succ=0;

	filec_set_act(&LF, streamPtr);

	if (filec_active(&LF))
		{
		LF.count_fclose++;
		succ = fclose(streamPtr);
		filec_dispose_act(&LF);
		}
	else
		{
		fprintf(CHECKER_OUT, "\n\nCHECKER: CHYBA\n------------------------------\n"
							 "Volanim funkce\nint fclose(FILE*)\nse pokousite uzavrit soubor, ktery nebyl otevren\n\n");
		printchar(0, PAGE_WIDTH, '-');
		fprintf(CHECKER_OUT, "\nLokace chyby:\n"
							 "Zdrojovy soubor:radek: %s:%u\n", file, line);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", func);
		program_error();
		}
	return succ;
	}


/*********************************************************/
/*                  PRACE SE SEZNAMEM                    */
/*********************************************************/

void checkdel(void)
	{
	/*
	tato funkce je pridavana do funkce exit
	v pripade, ze v programu nenastane fatalni chyba (uvolneni nealokovane pameti apod.),
	vypise statistiky programu pomoci print_list() a nasledne uvolni nevolnenou pamet
	a uzavre neuzavrene soubory pomoci funkce dispose_lists()
	*/
#ifdef __cplusplus
	if(chk_delete_err_ptr)
		{
		fprintf(CHECKER_OUT, "------------------------------\n"
				"Lokace chyby (odalokace adresy: %p):\n"
				"Zdrojovy soubor:radek: %s:%u\n", chk_delete_err_ptr, HIDDEN_SOURCE_FILE_NAME, 0);
		fprintf(CHECKER_OUT, "Volani funkce: %s()\n", HIDDEN_FUNC_NAME);
		fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: %u\n", L.count_dealloc);
		}
	chk_delete_err_ptr = CHECKER_NULL;	
#endif /* __cplusplus */
	
	if (correct_exit == CHECKER_TRUE) 
		print_lists(&L, &LF, CHECKER_TRUE, CHECKER_TRUE);
	dispose_lists(&L, &LF);
	}

void allocc_init_list(struct TListMem *aLm) 
	{
	/* inicializuje seznam pro alokace*/

	aLm->act = CHECKER_NULL;
	aLm->first = CHECKER_NULL;
	aLm->count_alloc = 0;
	aLm->count_realloc = 0;
	aLm->count_dealloc = 0;
	aLm->size_alloc_max_peak = 0;
	aLm->size_alloc = 0;
	aLm->size_free = 0;
	}

void filec_init_list (struct TListFile *aLf) 
	{
	/* inicializuje seznam pro soubory*/

	aLf->act = CHECKER_NULL;
	aLf->first = CHECKER_NULL;
	aLf->count_fopen = 0;
	aLf->count_fclose = 0;
	}

void allocc_insert(struct TListMem *L, size_t size, void* address, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__], enum TAllocType type_func) 
	{
	/* vlozi na konec seznamu prvek se zadanymi parametry
	   nastavi vlozeny prvek (tzn. posledni) jako aktualni
	*/

	size_t len;
	struct TNodeMem *insPtr;
		
	insPtr = (struct TNodeMem *) malloc(sizeof(struct TNodeMem));

	if (!insPtr) 
		program_error();
	else	
		{
		insPtr->size = size;			// alokovana velikost
		insPtr->linenum = line;			// radek zdrojoveho kodu (makro __LINE__)

		len = strlen(file) + 1;
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file, __CHECKER_MAXLENGTH_FILENAME__);

		insPtr->alloc_num = L->count_alloc + 1;	// kolikate volani mallocu to je
		insPtr->memory_address = address;	// adresa alokovane pameti
		CHECKER_STRNCPY_S(insPtr->func, __CHECKER_MAXLENGTH_FUNCNAME__, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);		// jmeno nadrazene funkce (makro __CHECKER_FUNCNAME__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';

		insPtr->type_func = type_func;

		insPtr->ptr = CHECKER_NULL;

		// vlozeni do seznamu
		if(L->first)
			{
			// vlozeni na konec
			allocc_set_last_act(L);
			L->act->ptr = insPtr;
			L->act = insPtr;
			}
		else
			{
			L->first = insPtr;
			L->act = insPtr;
			}
		}
	
	/* aktualizace statistik */
	L->count_alloc++;
	L->size_alloc += size;
	if(L->size_alloc_max_peak < (L->size_alloc - L->size_free))
		L->size_alloc_max_peak = (L->size_alloc - L->size_free);
	}

void filec_insert(struct TListFile *L, const char aPath[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FILENAME__], FILE *stream) {  // ok
/*
vlozi na konec seznamu prvek se zadanymi parametry
nastavi vlozeny prvek (tzn. posledni) jako aktualni
*/
	size_t len;
	struct TNodeFile *insPtr;
	
	insPtr = (struct TNodeFile *) malloc(sizeof(struct TNodeFile));

	if (!insPtr)
		program_error();
	else
		{
		len = strlen(file) + 1;		// jmeno souboru (makro __FILE__)
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file, __CHECKER_MAXLENGTH_FILENAME__);

		CHECKER_STRNCPY_S(insPtr->func, __CHECKER_MAXLENGTH_FUNCNAME__, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1); // jmeno nadrazene funkce (makro __CHECKER_FUNCNAME__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';

		insPtr->linenum = line;				// radek zdrojoveho kodu (makro __LINE__)

		len = strlen(aPath) + 1; 		// jmeno oteviraneho souboru
		if(len > __CHECKER_MAXLENGTH_PATHNAME__ )
			CHECKER_STRNCPY_S(insPtr->path, __CHECKER_MAXLENGTH_PATHNAME__, aPath + len - __CHECKER_MAXLENGTH_PATHNAME__, __CHECKER_MAXLENGTH_PATHNAME__);
		else
			CHECKER_STRNCPY_S(insPtr->path, __CHECKER_MAXLENGTH_PATHNAME__, aPath, __CHECKER_MAXLENGTH_PATHNAME__);
				
		CHECKER_STRNCPY_S(insPtr->mode, __CHECKER_MAXLENGTH_FILEMODE__, mode, __CHECKER_MAXLENGTH_FILEMODE__ - 1);	// mod otvirani souboru
		insPtr->mode[__CHECKER_MAXLENGTH_FILEMODE__ - 1] = '\0';
		
		insPtr->memory_address = stream;				// adresa streamu

		insPtr->ptr = CHECKER_NULL;

		filec_set_last_act(L);			// vlozeni do seznamu
		if(L->act != CHECKER_NULL)
			{
			L->act->ptr = insPtr;
			L->act = insPtr;
			}
		else
			{
			L->first = insPtr;
			L->act = insPtr;
			}
		}
	}

void allocc_resize_act(struct TListMem *L, size_t size, void* address, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__], enum TAllocType type_func) 
	{
/* 
Aktualni prvek je naplnen novymi parametry vyuzivano pro funkci realloc()
ponecha aktivitu na zvolenem prvku
*/
	size_t len;
	struct TNodeMem *insPtr = L->act;

	if(allocc_active(L))
		{
		/* aktualizace statistik */
		L->count_realloc++;
		L->size_free  += L->act->size;
		L->size_alloc += size;
		if(L->size_alloc_max_peak < (L->size_alloc - L->size_free))
			L->size_alloc_max_peak = (L->size_alloc - L->size_free);
		
		insPtr->size = size;			// alokovana velikost
		insPtr->linenum = line;			// radek zdrojoveho kodu (makro __LINE__)
		
		len = strlen(file) + 1;								// jmeno souboru (makro __FILE__)
		if(len > __CHECKER_MAXLENGTH_FILENAME__ )
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file + len - __CHECKER_MAXLENGTH_FILENAME__, __CHECKER_MAXLENGTH_FILENAME__);
		else
			CHECKER_STRNCPY_S(insPtr->file, __CHECKER_MAXLENGTH_FILENAME__, file, __CHECKER_MAXLENGTH_FILENAME__);

		insPtr->alloc_num = L->count_alloc + 1;	// kolikate volani mallocu to je
		insPtr->memory_address = address;	// adresa alokovane pameti
		CHECKER_STRNCPY_S(insPtr->func, __CHECKER_MAXLENGTH_FUNCNAME__, func, __CHECKER_MAXLENGTH_FUNCNAME__ - 1);		// jmeno nadrazene funkce (makro __CHECKER_FUNCNAME__)
		insPtr->func[__CHECKER_MAXLENGTH_FUNCNAME__ - 1] = '\0';

		insPtr->type_func = type_func;				
		}
	else
		program_error();

	}

void allocc_dispose_act(struct TListMem *aLm) 
	{
	/*
	zrusi aktivni prvek. 
	Vazba z predchoziho se navaze na nasledujici prvek seznamu
	nastavi aktivitu na prvni prvek
	*/	
	if(allocc_active(aLm))
		{
		/* aktualizace statistik */
		aLm->count_dealloc++;
		aLm->size_free += aLm->act->size;
		
		if(aLm->act == aLm->first)	// jestli-ze je prvni
			aLm->first = aLm->act->ptr;
		else
			{						// jestli neni prvni, resim vazby
			struct TNodeMem *pom = aLm->first;

			while(pom->ptr != aLm->act) pom = pom->ptr;

			pom->ptr = aLm->act->ptr;
			}
		free(aLm->act);
		aLm->act = aLm->first;
		}
	else
		program_error();

	}

void filec_dispose_act(struct TListFile *aLf) 
	{
	/*
	zrusi aktivni prvek. 
	Vazba z predchoziho se navaze na nasledujici prvek seznamu
	nastavi aktivitu na prvni prvek
	*/
	if(filec_active(aLf))
		{
		if(aLf->act == aLf->first)  // jestli-ze je prvni
			aLf->first = aLf->act->ptr;
		else
			{						// jestli neni prvni, resim vazby
			struct TNodeFile *pom = aLf->first;

			while(pom->ptr != aLf->act) pom = pom->ptr;

			pom->ptr = aLf->act->ptr;
			}
		free(aLf->act);
		aLf->act = aLf->first;
		}
	}

void allocc_set_last_act(struct TListMem *aLm) 
	{
	/*
	nastavi jako aktualni prvek posledni prvek
	jestli-ze nastane chyba, nastavi CHECKER_NULL
	*/
	if(aLm->first) 
		{
		if (!allocc_active(aLm)) aLm->act = aLm->first;
		while(aLm->act->ptr) aLm->act = aLm->act->ptr;
		}
	else
		aLm->act = CHECKER_NULL;
	}

void filec_set_last_act (struct TListFile *aLf) 
	{
	/*
	nastavi jako aktualni prvek posledni prvek
	jestli-ze nastane chyba, nastavi CHECKER_NULL
	*/
	if (aLf->first) 
		{	
		if (!filec_active(aLf)) aLf->act = aLf->first;
		while(aLf->act->ptr) aLf->act = aLf->act->ptr;
		}
	else
		aLf->act = CHECKER_NULL;
	}

void allocc_set_act(struct TListMem *aLm, void *aAddress) 
	{
	/*
	najde prvek se zadanou adresou. V pripade, 
	ze takovy prvek nenajde, nastavi aLm->act = CHECKER_NULL
	*/	
	for(aLm->act = aLm->first; (aLm->act) && (aLm->act->memory_address != aAddress);  aLm->act = aLm->act->ptr) ;	
	}

void filec_set_act(struct TListFile *aLf, FILE *aStream) 
	{
	/*
	najde prvek se zadanou adresou. V pripade, 
	ze takovy prvek nenajde, nastavi aLf->act = CHECKER_NULL
	*/	
	for(aLf->act = aLf->first; (aLf->act) && (aLf->act->memory_address != aStream);  aLf->act = aLf->act->ptr) ;
	}

CHECKER_BOOL allocc_active(const struct TListMem *aLm)
	{
	/* jestli-ze je seznam aktivni, vraci 1, jinak 0 */
	return ((aLm->act) ? (CHECKER_TRUE) : (CHECKER_FALSE) );
	}

CHECKER_BOOL filec_active(const struct TListFile *aLf)
	{
	/* jestli-ze je seznam aktivni, vraci 1, jinak 0 */
	return ((aLf->act) ? (CHECKER_TRUE) : (CHECKER_FALSE) );
	}

void dispose_lists(struct TListMem* aLm, struct TListFile *aLf) 
	{		
	struct TNodeMem *pom;
	struct TNodeFile *pomf;

	while(aLm->first) 
		{
		pom = aLm->first;
		aLm->first = aLm->first->ptr;
		free(pom->memory_address); 	// uvolnim pamet alokovanou v programu
		free(pom);			// zrusim zaznam ze seznamu
		}
	aLm->first = CHECKER_NULL;
	aLm->act = CHECKER_NULL;

	while(aLf->first) 
		{
		pomf = aLf->first;
		aLf->first = aLf->first->ptr;
		fclose(pomf->memory_address);	// uzavru soubor
		free(pomf);			// zrusim zaznam ze seznamu
		}
	aLf->first = CHECKER_NULL;
	aLf->act = CHECKER_NULL;
	}

void program_error(void)
	{
	fprintf(CHECKER_OUT, "\nProgram musel byt ukoncen\n");
	fflush(CHECKER_OUT);
	correct_exit = CHECKER_FALSE;
	exit(EXIT_FAILURE);
	}

void allocc_print_list(struct TListMem *L)
	{
	size_t a = 0, b = 0, c = 0, e = 0, g = 0, h=0;
	size_t mezer_mezi_sloupci, x = 0;
	size_t pflen = 0; /* printf length */

	fprintf(CHECKER_OUT, "\n\nCHECKER: KONTROLNI VYPIS \n");
	printchar(0, PAGE_WIDTH, '=');
	putc('\n', CHECKER_OUT);

	if (!used_alloc)
		{
		fprintf(CHECKER_OUT, "\nZadna pamet nebyla dynamicky alokovana\n\n");
		return;
		}

	fprintf(CHECKER_OUT, "VYPIS ALOKOVANE PAMETI\n");
	fprintf(CHECKER_OUT, "Celkovy pocet alokaci pameti v programu: \t%u\n", L->count_alloc);
	if(L->count_realloc)
		fprintf(CHECKER_OUT, "Celkovy pocet realokaci pameti v programu: \t%u\n", L->count_realloc);
	
	fprintf(CHECKER_OUT, "Celkovy pocet uspesnych uvolneni pameti v programu: \t%u\n", L->count_dealloc);

	fprintf(CHECKER_OUT, "Spickove mnozstvi alokovane pameti pri behu programu:  " CHECKER_SIZE_T_PRIu "  bytu.\n", (CHECKER_SIZE_T_CONV)L->size_alloc_max_peak);
	fprintf(CHECKER_OUT, "Alokovano celkem  " CHECKER_SIZE_T_PRIu "  bytu, uvolneno  " CHECKER_SIZE_T_PRIu "  bytu\n", (CHECKER_SIZE_T_CONV)L->size_alloc, (CHECKER_SIZE_T_CONV)L->size_free);

	if (!L->first)
		{
		fprintf(CHECKER_OUT, "Vsechna dynamicka pamet programu byla bez chyby uvolnena. :-)\n\n");
		return;
		}

	fprintf(CHECKER_OUT, "NEODALOKOVANO ZUSTAVA  " CHECKER_SIZE_T_PRId "  BYTU !!!\n\n", (CHECKER_SIZE_T_CONV)L->size_alloc - (CHECKER_SIZE_T_CONV)L->size_free);

	/* ziskam delky nejdelsich udaju */
	L->act = L->first;
	while(L->act) 
		{
		#ifdef __CHECKER_TAB_INDEX__
		/* poradi alokace */
			x = chars_in_dec(L->act->alloc_num);
			if(x > a) a = x;
		#endif
		
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* filename */
			x = CHECKER_STRNLEN_S(L->act->file, __CHECKER_MAXLENGTH_FILENAME__);
			if(x > b) b = x;
		/* line number starting from 0!!!!*/
			x = chars_in_dec(L->act->linenum);
			if(x > c) c = x;
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* function name */
			x = CHECKER_STRNLEN_S(L->act->func, __CHECKER_MAXLENGTH_FUNCNAME__);
		/* allocation type (malloc/new/realloc) */
			x += CHECKER_STRNLEN_S(alloc_type_str[L->act->type_func % ECount], CHECKER_MAXLENGTH_ALLOC_TYPE_STR);
			if(x > h) h = x;
		#endif
		
		#ifdef __CHECKER_TAB_SIZE__
		/* allocated memory block size */
			x = chars_in_dec(L->act->size);
			if (x > e) e = x;
		#endif
	
		#ifdef __CHECKER_TAB_MEM_ADDR__ 
		/* Address can be 0!!!! (So, addr + 1) */
			x = 1 + 4; /* pro pomlcku a dvakrat znaky 0x */
			x += chars_in_hex((ptrdiff_t) L->act->memory_address);
			x += chars_in_hex((ptrdiff_t) ((char*)L->act->memory_address + L->act->size) );
			if (x > g) g = x;
		#endif
		
		L->act = L->act->ptr;
		} /* while(L->act) */

	x = a + b + c + e + h + g + 6;				/* prizpusobeni sirky tabulky */
	if (x < 35) mezer_mezi_sloupci = 8;
	else if(x < 40) mezer_mezi_sloupci = 7;
	else if(x < 45) mezer_mezi_sloupci = 6;
	else if(x < 50) mezer_mezi_sloupci = 5;
	else if(x < 55) mezer_mezi_sloupci = 4;
	else if(x < 60) mezer_mezi_sloupci = 3;
	else mezer_mezi_sloupci = 2;
				
	
	/* Zahlavi tabulky */
	fprintf(CHECKER_OUT, "VYPIS ALOKOVANE PAMETI\n");
	
	#ifdef __CHECKER_TAB_INDEX__
	/* Poradi alokace */
		pflen = fprintf(CHECKER_OUT, "#");
		if(a < pflen)
			a = pflen;
		a+= mezer_mezi_sloupci;
		printchar(pflen, a, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_FILE_LINENUM__
	/* Soubor:Radek */
		pflen = fprintf(CHECKER_OUT, "Soubor:Radek");
		if(b + c + 1 < pflen)
			b = pflen;
		else
			b += c + 1;
	
		b += mezer_mezi_sloupci;
		printchar(pflen, b, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_FUNCTION__
	/* Funkce */
		pflen = fprintf(CHECKER_OUT, "Funkce");

		if(h + 3 + 2 < pflen)
			h = pflen;
		else
			h += 3 + 2;
	
		h += mezer_mezi_sloupci;
		printchar(pflen, h, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_SIZE__
	/* allocated memblock size */
		pflen = fprintf(CHECKER_OUT, "B");
		if(e < pflen)
			e = pflen;
		e += mezer_mezi_sloupci;
		printchar(pflen, e, ' ');
	#endif
	
	#ifdef __CHECKER_TAB_MEM_ADDR__
		pflen = fprintf(CHECKER_OUT, "Adresa pameti");
		if(g < pflen)
			g = pflen;
		printchar(pflen, g, ' ');
	#endif
	
	/* Title break line */
	putc('\n', CHECKER_OUT);
	#ifdef __CHECKER_TAB_INDEX__
		printchar(mezer_mezi_sloupci, a, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FILE_LINENUM__
		printchar(mezer_mezi_sloupci, b, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FUNCTION__
		printchar(mezer_mezi_sloupci, h, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_SIZE__
		printchar(mezer_mezi_sloupci, e, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_MEM_ADDR__
		printchar(0, g, '-');
	#endif	
	putc('\n', CHECKER_OUT);
	
	/* Telo tabulky */
	L->act = L->first;
	while(L->act) 
		{
		#ifdef __CHECKER_TAB_INDEX__
		/* poradi alokace */
			pflen = fprintf(CHECKER_OUT, "%u", L->act->alloc_num);
			printchar(pflen, a, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* file: line */
			pflen = fprintf(CHECKER_OUT, "%s:%u", L->act->file, L->act->linenum);
			printchar(pflen, b, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* function name and allocation type */
			pflen = 0;
			if(CHECKER_STRNLEN_S(L->act->func, __CHECKER_MAXLENGTH_FUNCNAME__))
				{
				pflen = fprintf(CHECKER_OUT, "%s()", L->act->func);
				}
			pflen += fprintf(CHECKER_OUT, ":%s()", alloc_type_str[L->act->type_func]);

			printchar(pflen, h, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_SIZE__
			/* velikost */
			pflen = fprintf(CHECKER_OUT, CHECKER_SIZE_T_PRIu, (CHECKER_SIZE_T_CONV)L->act->size);
			printchar(pflen, e, ' ');
		#endif
		
		#ifdef __CHECKER_TAB_MEM_ADDR__
			#if (_MSC_VER >= 1310)  /* od VS2003 definovan modifikator velikosti (%Iu), ktery je bud I32 nebo I64 */
				pflen = fprintf(CHECKER_OUT, "0x%Ix-0x%Ix", (ptrdiff_t)L->act->memory_address, (ptrdiff_t)((char*)L->act->memory_address + L->act->size));
			#else
				pflen = fprintf(CHECKER_OUT, "%p-%p", L->act->memory_address, (void*)((char*)L->act->memory_address + L->act->size));
			#endif
			printchar(pflen, g, ' ');
		#endif
		
		putc('\n', CHECKER_OUT);
		L->act = L->act->ptr;
		} /* while(L->act) */
	
	putc('\n', CHECKER_OUT);
	} /* allocc_print_lists() */


void filec_print_lists(struct TListFile *LF)
	{
	size_t b = 0, c = 0, d = 0, e = 0, f = 0;
	size_t mezer_mezi_sloupci, x = 0;
	size_t pflen = 0; /* printf length */
	
	if(!used_file_c)
		{
		fprintf(CHECKER_OUT, "\nZadne soubory nebyly v programu zpracovavany.\n\n");
		return;
		}

	fprintf(CHECKER_OUT, "PREHLED OTEVRENYCH SOUBORU\n");
	fprintf(CHECKER_OUT, "Pocet otevrenych souboru: \t%u\n", LF->count_fopen);
	fprintf(CHECKER_OUT, "Pocet uzavrenych souboru: \t%u\n", LF->count_fclose);
	fprintf(CHECKER_OUT, "NEUZAVRENYCH ZUSTAVA  %d  SOUBORU\n\n", LF->count_fopen - LF->count_fclose);
	
	if (!LF->first)
		{
		fprintf(CHECKER_OUT, "Vsechny soubory byly bez chyby uzavreny\n\n");
		return;
		}
		
	/* ziskam delky nejdelsich udaju */
	LF->act = LF->first;

	while(LF->act)
		{
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* soubor se zdrojovym kodem */
			x = CHECKER_STRNLEN_S(LF->act->file, __CHECKER_MAXLENGTH_FILENAME__);
			if(x > b) b = x;
		/* cislo radku */
			x = chars_in_dec(LF->act->linenum);	
			if(x > c) c = x;
		#endif
		
		#ifdef __CHECKER_TAB_FUNCTION__
		/* funkce */
			x = CHECKER_STRNLEN_S(LF->act->func, __CHECKER_MAXLENGTH_FUNCNAME__);
			if(x > d) d = x;
		#endif
		
		/* otevirany soubor */
		x = CHECKER_STRNLEN_S(LF->act->path, __CHECKER_MAXLENGTH_PATHNAME__);
		if(x > e) e = x;
		
		/* mod otevreni */
		x = CHECKER_STRNLEN_S(LF->act->mode, __CHECKER_MAXLENGTH_FILEMODE__);
		if(x > f) f = x;
		
		LF->act = LF->act->ptr;
		} /* while(LF->act) */

	x = b + c + d + e + f;						/* prizpusobeni sirky tabulky */
	if(x < 25) mezer_mezi_sloupci = 8;
	else if(x < 30) mezer_mezi_sloupci = 7;
	else if(x < 35) mezer_mezi_sloupci = 6;
	else if(x < 40) mezer_mezi_sloupci = 5;
	else if(x < 45) mezer_mezi_sloupci = 4;
	else if(x < 50) mezer_mezi_sloupci = 3;
	else mezer_mezi_sloupci = 2;
	
	/* zahlavi tabulky */
	fprintf(CHECKER_OUT, "VYPIS OTEVRENYCH SOUBORU\n");

	#ifdef __CHECKER_TAB_FILE_LINENUM__
		pflen = fprintf(CHECKER_OUT, "Misto otevreni");		/* soubor: radek */
		if(b + c + 1 < pflen)
			b = pflen;
		else
			b += c + 1;
		b += mezer_mezi_sloupci;
		printchar(pflen, b, ' ');
	#endif
			
	#ifdef __CHECKER_TAB_FUNCTION__
		if(d)
			{
			pflen = fprintf(CHECKER_OUT, "Funkce");			/* funkce */
			d += 2 + mezer_mezi_sloupci;
			printchar(pflen, d, ' ');
			}
	#endif
			
	pflen = fprintf(CHECKER_OUT, "Otevreny soubor");		/* Jmeno souboru */
	if(e < pflen)
		e = pflen;
	e += mezer_mezi_sloupci;
	printchar(pflen, e, ' ');
		
	pflen = fprintf(CHECKER_OUT, "Mod otevreni");		/* Mod otevreni */
	if(f < pflen)
		f = pflen;
	printchar(pflen, f, ' ');

	/* Title break line */
	putc('\n', CHECKER_OUT);
	#ifdef __CHECKER_TAB_FILE_LINENUM__
		printchar(mezer_mezi_sloupci, b, '-');
		printchar(0, mezer_mezi_sloupci, ' ');
	#endif
	#ifdef __CHECKER_TAB_FUNCTION__
		if(d)
			{
			printchar(mezer_mezi_sloupci, d, '-');
			printchar(0, mezer_mezi_sloupci, ' ');
			}
	#endif
	printchar(mezer_mezi_sloupci, e, '-');
	printchar(0, mezer_mezi_sloupci, ' ');

	printchar(0, f, '-');
	putc('\n', CHECKER_OUT);
	
	/* Telo tabulky */
	LF->act = LF->first;
			
	while(LF->act)
		{
		#ifdef __CHECKER_TAB_FILE_LINENUM__
		/* file: line */
			pflen = fprintf(CHECKER_OUT, "%s:%u", LF->act->file, LF->act->linenum);
			printchar(pflen, b, ' ');
		#endif
				
		#ifdef __CHECKER_TAB_FUNCTION__
		/* funkce */
		if(d)
			{
			pflen = fprintf(CHECKER_OUT, "%s()", LF->act->func);
			printchar(pflen, d, ' ');
			}
		#endif
				
		/* nazev souboru */
		pflen = fprintf(CHECKER_OUT, "%s", LF->act->path);
		printchar(pflen, e, ' ');
				
		/* mod otevreni */
		pflen = fprintf(CHECKER_OUT, "%s", LF->act->mode);
		printchar(pflen, f, ' ');

		putc('\n', CHECKER_OUT);
		LF->act = LF->act->ptr;
		} /* while(LF->act) */
			
	putc('\n', CHECKER_OUT);
	} /* filec_print_lists() */


void print_lists(struct TListMem *aL, struct TListFile *aLF, CHECKER_BOOL aPrint_alloc, CHECKER_BOOL aPrint_file)
	{
	if(aPrint_alloc)
		allocc_print_list(aL);
	
	if(aPrint_alloc && aPrint_file && used_file_c)
		{
		putc('\n', CHECKER_OUT);
		printchar(0, PAGE_WIDTH, '-');
		putc('\n', CHECKER_OUT);
		}

	if(aPrint_file)
		filec_print_lists(aLF);

	fflush(CHECKER_OUT);
	} /* print_lists */

void printchar(size_t from, size_t to, char ch)
	{
	for(; from < to; from++) putc(ch, CHECKER_OUT);
	} /* printchar() */

unsigned int chars_in_dec(size_t val)
	{
	unsigned int i = 0;
	do
		{
		val /= 10;
		i++;
		}
	while(val);
	return(i);
	} /* chars_in_dec() */

unsigned int chars_in_hex(ptrdiff_t val)
	{
	unsigned int i = 0;
	do
		{
		val >>= 4;
		i++;
		}
	while(val);
	return(i);
	} /* chars_in_hex() */

/* End of file check.cpp */
