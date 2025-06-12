/*******************************************************************
**                                                                **
**  Knihovna pro kontrolu prace s alokovanou pameti a se soubory  **
**  Autor: Pavel Sabatka a Petr Petyovsky                         **
**  Implementovano v ramci bakalarske prace                       **
**  VUT v Brne, UAMT, FEKT v ak. roce 2007, 2008, 2010 - 2019     **
**  rel. 2012.25                                                  **
**                                                                **
*******************************************************************/
/* $Id: check.h 3 2022-10-26 17:03:14Z petyovsky $ */

/**
** NAVOD K POUZITI:
**	Pro pouziti teto knihovny je treba:
**	  1)  pripojit soubory check.h a check.c k projektu
**	  2)  pripojit check.h directivou #include. Tj. do 
**		  hlavickoveho souboru uvest
**			#include "check.h"
**
** UZIVATELSKA MAKRA:
**	Jsou definovany 3 uzivatelske makra, ktere lze volat z programu:
**		- void memory_stat(void); - aktualni vypis alokovane pameti
**		- void file_stat(void); - vypis otevrenych souboru v dane chvili
**		- void stat(void); - slucuje obe predchozi funkce
**/

#ifndef __CHECK_H__
#define __CHECK_H__

#if !defined(_CRT_SECURE_NO_DEPRECATE) && (defined(_MSC_VER) || defined(WIN32) || defined(_WIN64))
	#define _CRT_SECURE_NO_DEPRECATE 1		/* Due to MSVC Security development lifecycle (SDL) checks */
#endif

#define CHECKER_API extern

/* include knihoven */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
	#include <new>
#endif /* __cplusplus */

/*******************************
*   definice maker pro check   *
*******************************/
/* presmerovani vystupu checkeru standardni / chybovy vystup */
#define CHECKER_OUT stderr

/* delka nazvu funkci a souboru */
#define __CHECKER_MAXLENGTH_FILENAME__ 25
#define __CHECKER_MAXLENGTH_FUNCNAME__ 20
#define __CHECKER_MAXLENGTH_PATHNAME__ 25
#define __CHECKER_MAXLENGTH_FILEMODE__ 10

/* sloupce tabulky vypisu - pri odstraneni definice makra se sloupec nezobrazi */
#define __CHECKER_TAB_INDEX__
#define __CHECKER_TAB_FILE_LINENUM__
#define __CHECKER_TAB_FUNCTION__
#define __CHECKER_TAB_SIZE__
#define __CHECKER_TAB_MEM_ADDR__ 

/* makro __func__  je definovano pouze v C99 a C++11 */
/* ISO/IEC N3242=11-0012 [dcl.fct.def.general]
 * It is unspecified whether such a variable __func__has an address distinct from 
 * that of any other object in the program.
 */ 
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L)
	#define __CHECKER_FUNCNAME__ __func__
#elif (_MSC_VER >= 1500) /* >= Visual Studio 2008 */
	#define __CHECKER_FUNCNAME__ __FUNCTION__
#else
	#define __CHECKER_FUNCNAME__ ""
#endif /* (__STDC_VERSION__ >= 199901L) || (__cplusplus >= 201103L) */

/* definice maker pro funkce */
#define malloc(aSize)				chk_malloc((aSize), __LINE__, __FILE__, __CHECKER_FUNCNAME__)
#define calloc(aNum, aSize)			chk_calloc((aNum), (aSize), __LINE__, __FILE__, __CHECKER_FUNCNAME__)
#define realloc(aPtr, aSize)		chk_realloc((aPtr), (aSize), __LINE__, __FILE__, __CHECKER_FUNCNAME__)
#define free(aPtr)					chk_free((aPtr), __LINE__, __FILE__, __CHECKER_FUNCNAME__)
#define fopen(aFileName, aMode)		chk_fopen((aFileName), (aMode), __LINE__, __FILE__, __CHECKER_FUNCNAME__)
#define fclose(aFile)				chk_fclose((aFile), __LINE__, __FILE__, __CHECKER_FUNCNAME__)

/* uzivatelske makra */
/* vypise aktualni alokovanou pamet a otevrene soubory */
#define stat()						chk_stat(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vypise aktualni alokovanou pamet */
#define memory_stat()				chk_memory_stat(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vypise aktualni otevrene soubory */
#define file_stat()					chk_file_stat(__LINE__, __FILE__, __CHECKER_FUNCNAME__)

/* vraci nejvyssi vrchol alokovane pameti (od spusteni programu az po soucasnost) */
#define get_alloc_max_peak_size()	chk_get_alloc_max_peak_size(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci sumu celkove naalokovane pameti */
#define get_alloc_total_size()		chk_get_alloc_total_size(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci sumu celkove dealokovane pameti */
#define get_dealloc_total_size()	chk_get_dealloc_total_size(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci velikost aktualne alokovane pameti */
#define get_alloc_actual_size()		chk_get_alloc_actual_size(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci celkovy pocet pozadavku na alokaci pameti */
#define get_alloc_count()			chk_get_alloc_count(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci celkovy pocet pozadavku na realokaci pameti */
#define get_realloc_count()			chk_get_realloc_count(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci celkovy pocet pozadavku na dealokaci pameti */
#define get_dealloc_count()			chk_get_dealloc_count(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci celkovy pocet pozadavku na otevreni souboru */
#define get_fopen_count()			chk_get_fopen_count(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* vraci celkovy pocet pozadavku na zavreni souboru */
#define get_fclose_count()			chk_get_fclose_count(__LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* Aktivace zamku na funkcnich alokaci, vsechny alokace vetsi nez aAfterLimit budou hlasit chybu alokace, funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
#define lock_alloc(aAfterLimit) chk_lock_alloc(aAfterLimit, __LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* Odemknuti zamku alokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
#define unlock_alloc(aAlloc_lock_key) chk_unlock_alloc(aAlloc_lock_key, __LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* Aktivace zamku na funkcnich realokaci, vsechny realokace vetsi nez aAfterLimit budou hlasit chybu realokace, (mimo volani funkce realloc ktery funguji jako free), funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
#define lock_realloc(aAfterLimit) chk_lock_realloc(aAfterLimit, __LINE__, __FILE__, __CHECKER_FUNCNAME__)
/* Odemknuti zamku realokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
#define unlock_realloc(aRealloc_lock_key) chk_unlock_realloc(aRealloc_lock_key, __LINE__, __FILE__, __CHECKER_FUNCNAME__)

#ifdef __cplusplus
	extern "C"
		{
#endif /* __cplusplus */

/* uzivatelske funkce */
/* vypise aktualni alokovanou pamet a otevrene soubory */
CHECKER_API void chk_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vypise aktualni alokovanou pamet */
CHECKER_API void chk_memory_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vypise aktualni otevrene soubory */
CHECKER_API void chk_file_stat(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

/* vraci nejvyssi vrchol alokovane pameti (od spusteni programu az po soucasnost) */
CHECKER_API size_t chk_get_alloc_max_peak_size(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci sumu celkove naalokovane pameti */
CHECKER_API size_t chk_get_alloc_total_size(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci sumu celkove dealokovane pameti */
CHECKER_API size_t chk_get_dealloc_total_size(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci velikost aktualne alokovane pameti */
CHECKER_API size_t chk_get_alloc_actual_size(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci celkovy pocet pozadavku na alokaci pameti */
CHECKER_API unsigned int chk_get_alloc_count(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci celkovy pocet pozadavku na realokaci pameti */
CHECKER_API unsigned int chk_get_realloc_count(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci celkovy pocet pozadavku na dealokaci pameti */
CHECKER_API unsigned int chk_get_dealloc_count(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci celkovy pocet pozadavku na otevreni souboru */
CHECKER_API unsigned int chk_get_fopen_count(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* vraci celkovy pocet pozadavku na zavreni souboru */
CHECKER_API unsigned int chk_get_fclose_count(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

/* Aktivace zamku na funkcnich alokaci, vsechny alokace vetsi nez aAfterLimit budou hlasit chybu alokace, funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
CHECKER_API size_t chk_lock_alloc(unsigned int aAfterLimit, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* Odemknuti zamku alokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
CHECKER_API size_t chk_unlock_alloc(size_t aAlloc_lock_key, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* Aktivace zamku na funkcnich realokaci, vsechny realokace vetsi nez aAfterLimit budou hlasit chybu realokace, (mimo volani funkce realloc ktery funguji jako free), funkce vraci klic pomoci ktereho je mozne zamek opet deaktivovat */
CHECKER_API size_t chk_lock_realloc(unsigned int aAfterLimit, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
/* Odemknuti zamku realokaci, parametrem je hodnota klice vracena pri volani funkce pro zakmuti */
CHECKER_API size_t chk_unlock_realloc(size_t aRealloc_lock_key, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

/*  kontrolni funkce */
CHECKER_API void* chk_malloc(size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
CHECKER_API void* chk_calloc(size_t count, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
CHECKER_API void* chk_realloc(void* realloc_ptr, size_t size, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
CHECKER_API void chk_free(void *Ptr, unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char name_fce[__CHECKER_MAXLENGTH_FUNCNAME__]);

CHECKER_API FILE* chk_fopen(const char file[__CHECKER_MAXLENGTH_PATHNAME__], const char mode[__CHECKER_MAXLENGTH_FILEMODE__], unsigned int line, const char filesource[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);
CHECKER_API int chk_fclose(FILE* streamPtr, unsigned int line, const char file[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

/* Push checker's hooked functions to the std namespace */
#ifdef __cplusplus
	namespace std
		{
		using ::chk_malloc;
		using ::chk_calloc;
		using ::chk_realloc;
		using ::chk_free;
		using ::chk_fopen;
		using ::chk_fclose;
		} /* namespace std */
#endif /* __cplusplus */

#ifdef __cplusplus
		} /* extern "C" */
#endif /* __cplusplus */

/* KONTROLA PAMETI PRO C++ */
#ifdef __cplusplus

#if (__cplusplus >= 201103L) || (defined(_MSC_VER) && (_MSC_VER >= 1900))		/* C++11 and newer or Visual Studio 2015 and newer */
	#define _CHECKER_BAD_ALLOC_EXCEPTION
	#define _CHECKER_NO_EXCEPTION			noexcept

#elif (__cplusplus < 201103L) && (__cplusplus >= 199711L) && !defined(_MSC_VER) || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 7)) /* C++98 and not Visual Studio or gcc (4.0-4.6) */
	#define _CHECKER_BAD_ALLOC_EXCEPTION		throw(std::bad_alloc)
	#define _CHECKER_NO_EXCEPTION			throw()

#elif (defined(_MSC_VER) && (_MSC_VER <= 1800)) || defined(WIN32) || defined(_WIN64)	/* up to Visual Studio 2013 not newer */
	#define _CHECKER_BAD_ALLOC_EXCEPTION			/* Must be empty, otherwise MSVC report: warning C4290: C++ exception specification ignored except to indicate a function is not __declspec(nothrow) */
	#define _CHECKER_NO_EXCEPTION			throw()

#else										/* Pre C++98 */
	#define _CHECKER_BAD_ALLOC_EXCEPTION
	#define _CHECKER_NO_EXCEPTION
#endif

	/* kontrolni operatory */
	CHECKER_API void* operator new(size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION;
	CHECKER_API void* operator new[](size_t aSize) _CHECKER_BAD_ALLOC_EXCEPTION;
	CHECKER_API void* operator new(size_t aSize, const std::nothrow_t& aNoThrow) _CHECKER_NO_EXCEPTION;
	CHECKER_API void* operator new[](size_t aSize, const std::nothrow_t& aNoThrow) _CHECKER_NO_EXCEPTION;

	CHECKER_API void operator delete(void *aPtr) _CHECKER_NO_EXCEPTION;
	CHECKER_API void operator delete[](void *aPtr) _CHECKER_NO_EXCEPTION;

	#if (__cplusplus >= 201402L) || (_MSC_FULL_VER >= 190023918L)		/* C++14 global sized dealocation feature */
		CHECKER_API void operator delete(void *aPtr, size_t aSize) _CHECKER_NO_EXCEPTION;
		CHECKER_API void operator delete[](void *aPtr, size_t aSize) _CHECKER_NO_EXCEPTION;
	#endif

	CHECKER_API bool chk_new_recorder(unsigned int line, const char soubor[__CHECKER_MAXLENGTH_FILENAME__], const char func[__CHECKER_MAXLENGTH_FUNCNAME__]);

	/* pomocna trida pro ulozeni zaznamu pri chybe v delete */
	class CChkDeleteRecorder
		{
		const char * const iFile;
		const char * const iFunc;
		const unsigned iLine;
		
	public:
		CChkDeleteRecorder(const unsigned aLine, const char * const aFile, const char * const aFunc): iFile(aFile), iFunc(aFunc), iLine(aLine)
			{}
		~CChkDeleteRecorder();
		operator bool() const
			{return((!iFile) && (!iFunc));} /* Always return false! No so easy to understand for compiler optimizer. I hope. */
		};

	/* definice maker pro operatory */
	#define _CHECKER_NEW_OPER chk_new_recorder(__LINE__, __FILE__, __CHECKER_FUNCNAME__) ? 0 : new
	#define new _CHECKER_NEW_OPER

	#define _CHECKER_DELETE_OPER CChkDeleteRecorder(__LINE__, __FILE__, __CHECKER_FUNCNAME__) ? (void)0 : (void) delete
	#define delete _CHECKER_DELETE_OPER

#endif /* __cplusplus */

#endif /* __CHECK_H__ */
