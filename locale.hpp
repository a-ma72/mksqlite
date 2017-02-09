/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      locale.hpp
 *  @brief     (Error-)messages in english and german.
 *  @details   All text strings omitted by mksqlite are cumulated in this file
 *             for the case of further translations.
 *  @authors   Martin Kortmann <mail@kortmann.de>, 
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.5
 *  @date      2008-2017
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

#include "config.h"
//#include "global.hpp"
#include "svn_revision.h" /* get the SVN revision number */
#include <cstdarg>
extern "C"
{
  #include "blosc/blosc.h"
}

/* Localization, declaration */

const char*   getLocaleMsg  ( int iMsgNr );
bool          setLocale     ( int iLang );
int           getLocale     ();


/*
 * a poor man localization.
 * every language have a table of messages.
 */

/** 
 * \name Message Identifiers
 * \anchor MSG_IDS
 *
 * Most messages are identified by their identifiers. Each enumerated message
 * has a representation in english and in german.
 * 
 * Other messages, which have no translation have the identifier MSG_PURESTRING and
 * its text is taken in a buffer \ref Err::m_static_msg.
 *
 * @{
 */
#define MSG_PURESTRING                  -2
#define MSG_NOERROR                     -1
#define MSG_HELLO                        0
#define MSG_INVALIDDBHANDLE              1
#define MSG_IMPOSSIBLE                   2
#define MSG_USAGE                        3
#define MSG_INVALIDARG                   4
#define MSG_CLOSINGFILES                 5
#define MSG_CANTCOPYSTRING               6
#define MSG_NOOPENARG                    7
#define MSG_NOFREESLOT                   8
#define MSG_CANTOPEN                     9
#define MSG_DBNOTOPEN                   10
#define MSG_INVQUERY                    11
#define MSG_CANTCREATEOUTPUT            12
#define MSG_UNKNWNDBTYPE                13
#define MSG_BUSYTIMEOUTFAIL             14
#define MSG_MSGUNIQUEWARN               15
#define MSG_UNEXPECTEDARG               16
#define MSG_MISSINGARGL                 17
#define MSG_ERRMEMORY                   18
#define MSG_UNSUPPVARTYPE               19
#define MSG_UNSUPPTBH                   20   
#define MSG_ERRPLATFORMDETECT           21
#define MSG_WARNDIFFARCH                22
#define MSG_BLOBTOOBIG                  23
#define MSG_ERRCOMPRESSION              24
#define MSG_UNKCOMPRESSOR               25
#define MSG_ERRCOMPRARG                 26
#define MSG_ERRCOMPRLOGMINVALS          27
#define MSG_ERRUNKOPENMODE              28
#define MSG_ERRUNKTHREADMODE            29
#define MSG_ERRCANTCLOSE                30
#define MSG_ERRCLOSEDBS                 31
#define MSG_ERRNOTSUPPORTED             32
#define MSG_EXTENSION_EN                33
#define MSG_EXTENSION_DIS               34
#define MSG_EXTENSION_FAIL              35
#define MSG_MISSINGARG                  36
#define MSG_MISSINGARG_CELL             37
#define MSG_MISSINGARG_STRUCT           38
#define MSG_NUMARGEXPCT                 39
#define MSG_SINGLECELLNOTALLOWED        40
#define MSG_SINGLESTRUCTNOTALLOWED      41
#define MSG_ERRVARNAME                  42
#define MSG_STREAMINGNEEDTYBLOBS        43
#define MSG_STREAMINGNOTSUPPORTED       44
#define MSG_RESULTTYPE                  45
#define MSG_DBID_SUPFLOUS               46
#define MSG_FCNHARGEXPCT                47
#define MSG_LITERALARGEXPCT             48
#define MSG_RECURSIVECALL               49
#define MSG_INVALIDFUNCTION             50
#define MSG_ERRNULLDBID                 51
#define MSG_ERRINTERNAL                 52
#define MSG_ABORTED                     53
/** @}  */


/**
 * \brief Helperclass for error message transport
 *
 */
class Err
{
    int         m_msgId;              ///< Message ID (see \ref MSG_IDS "Message Identifiers")
    char        m_shared_msg[1024];   ///< (Shared) text buffer for non-const (generated) messages
    const char* m_static_msg;         ///< Holds pointer to static message text
    const char* m_err_id;             ///< Holds the error id (for MATLAB exception handling f.e., see \ref MSG_IDS)
    bool        m_isPending;          ///< Message has still to be handled if this flag is set
    
public:
    
    /// Constructor
    Err() 
    { 
        clear(); 
    }
    
    
    /// Reset error message
    void clear()
    {
        m_msgId        = MSG_NOERROR;
        m_static_msg   = m_shared_msg;
        m_err_id       = "";
        *m_shared_msg  = 0;     // not used and thus emptied
        m_isPending    = false;
    }

    
    /**
     * \brief Set error message to a constant string (without translation)
     *
     * \param[in] strMsg Pointer to constant error message text
     * \param[in] strId  Pointer to constant error identifier
     */
    void set( const char* strMsg, const char* strId = NULL )
    {
        if( !strMsg ) 
        {
            clear();
        }
        else
        {
            m_msgId       = MSG_PURESTRING;
            m_static_msg  = strMsg;
            m_err_id      = strId;
            *m_shared_msg = 0;     // not used and thus emptied
            m_isPending   = true;
        }
    }
    
    
    /** \brief Set error message to non-constant string (no translation)
     *
     * \param[in] strMsg Pointer to message text
     * \param[in] strId  Pointer to constant error identifier
     */
    void set( char* strMsg, const char* strId = NULL )
    {
        m_err_id = strId;
        
        if( !strMsg ) 
        { 
            clear();
        }
        else
        {
            m_msgId       = MSG_PURESTRING;
            m_static_msg  = m_shared_msg;
            m_isPending   = true;
            
            _snprintf( m_shared_msg, sizeof(m_shared_msg), "%s", strMsg );
        }
    }
    
    
    /** 
     * \brief Set error message by identifier (translations available)
     *
     * \param[in] iMessageNr Message identifier (see \ref MSG_IDS "Message Identifiers")
     * \param[in] strId  Pointer to constant error identifier
     */
    void set( int iMessageNr, const char* strId = NULL )
    {
         m_msgId = iMessageNr;
         
         if( iMessageNr == MSG_NOERROR )
         {
            clear();
         }
         else
         {
            set( ::getLocaleMsg( iMessageNr ), strId );
         }
    }
    
    
    /** 
     * \brief Set error message by identifier (translations available) with printf arguments
     *
     * \param[in] iMessageNr Message identifier (see \ref MSG_IDS "Message Identifiers")
     * \param[in] strId  Pointer to constant error identifier
     */
    void set_printf( int iMessageNr, const char* strId, ... )
    {
         va_list va;
         va_start( va, strId );
         const char* message = ::getLocaleMsg( iMessageNr );

         m_msgId = iMessageNr;
         *m_shared_msg = 0;

         if( message )
         {
            vsnprintf( m_shared_msg, sizeof( m_shared_msg ), message, va );
         }
         set( m_shared_msg, strId );
         
         va_end( va );
    }
    
    
    /** 
     * \brief Set error message by format string with arguments
     *
     * \param[in] fmt  Pointer to constant format string
     * \param[in] strId  Pointer to constant error identifier
     */
    void set_printf( const char* fmt, const char* strId, ... )
    {
         va_list va;
         va_start( va, strId );

         m_msgId = MSG_PURESTRING;
         *m_shared_msg = 0;

         if( fmt )
         {
            vsnprintf( m_shared_msg, sizeof( m_shared_msg ), fmt, va );
         }
         set( m_shared_msg, strId );
         
         va_end( va );
    }
    
    
    /**
     * \brief Get the current error message
     *
     * \param[in] errId Pointer to constant error identifier
     */
    const char* get( const char** errId = NULL )
    {
        if( errId )
        {
            *errId = m_err_id;
        }
        
        return m_static_msg;
    }
    
    
    /// Get the current message identifier
    int getMsgId()
    {
        return m_msgId;
    }
    
    
    /// Returns true, if the current error message is still not handled
    bool isPending()
    { 
        return m_isPending;
    }
    
    
    /// Omits a warning with the current error message
    void warn( int iMessageNr )
    {
        mexWarnMsgTxt( ::getLocaleMsg( iMessageNr ) );
    }

};



#ifdef MAIN_MODULE

/* Implementations */

/**
 * @brief Message table for english translations (\ref Language==0)
 */
static const char* messages_0[] = 
{
    "mksqlite Version " CONFIG_MKSQLITE_VERSION_STRING " " SVNREV ", an interface from MATLAB to SQLite\n"
    "(c) 2008-2017 by Martin Kortmann <mail@kortmann.de>\n"
    "                 Andreas Martin  <andimartin@users.sourceforge.net>\n"
    "based on SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite utilizes:\n"
    " - DEELX perl compatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " compression algorithm (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) implementation by Alexander Peslyak\n"
    "   \n",
    
/*  1*/    "invalid database handle",
/*  2*/    "function not possible",
/*  3*/    "usage: mksqlite([dbid,] command [, databasefile])\n",
/*  4*/    "no or wrong argument",
/*  5*/    "mksqlite: closing open databases",
/*  6*/    "can\'t copy string in getstring()",
/*  7*/    "open without database name",
/*  8*/    "no free database handle available",
/*  9*/    "cannot open database (check access privileges and existence of database)",
/* 10*/    "database not open",
/* 11*/    "invalid query string (semicolon?)",
/* 12*/    "cannot create output matrix",
/* 13*/    "unknown SQLITE data type",
/* 14*/    "cannot set busy timeout",
/* 15*/    "could not build unique field name for %s",
/* 16*/    "unexpected arguments passed",
/* 17*/    "missing argument list",
/* 18*/    "memory allocation error",
/* 19*/    "unsupported variable type",
/* 20*/    "unknown/unsupported typed blob header",
/* 21*/    "error while detecting the type of computer you are using",
/* 22*/    "BLOB stored on different type of computer",
/* 23*/    "BLOB exceeds maximum allowed size",
/* 24*/    "error while compressing data",
/* 25*/    "unknown compressor",
/* 26*/    "chosen compressor accepts 'double' type only",
/* 27*/    "chosen compressor accepts positive values only",
/* 28*/    "unknown open modus (only 'ro', 'rw' or 'rwc' accepted)",
/* 29*/    "unknown threading mode (only 'single', 'multi' or 'serial' accepted)",
/* 30*/    "cannot close connection",
/* 31*/    "not all connections could be closed",
/* 32*/    "this Matlab version doesn't support this feature",
/* 33*/    "extension loading enabled for this db",
/* 34*/    "extension loading disabled for this db",
/* 35*/    "failed to set extension loading feature",
/* 36*/    "more argument(s) expected",
/* 37*/    "more argument(s) expected (maybe matrix argument given, instead of a cell array?)",
/* 38*/    "missing field in argument for SQL parameter '%s'",
/* 39*/    "numeric argument expected",
/* 40*/    "single cell argument not allowed when streaming is enabled while multiple\nSQL parameters are used or parameter wrapping is enabled, too",
/* 41*/    "single struct argument not allowed when streaming is enabled while multiple\nSQL parameters are used or parameter wrapping is enabled, too",
/* 42*/    "unable to create fieldname from column name",
/* 43*/    "streaming of variables needs typed BLOBs! Streaming is off",
/* 44*/    "streaming not supported in this MATLAB version",
/* 45*/    "result type is ",
/* 46*/    "database ID is given, but superflous!",
/* 47*/    "function handle expected!",
/* 48*/    "string argument expected!",
/* 49*/    "recursive application-defined functions not allowed!",
/* 50*/    "invalid function!",
/* 51*/    "dbid of 0 only allowed for commands 'open' and 'close'!",
/* 52*/    "Internal error!",
/* 53*/    "Aborted (Ctrl+C)!",
};


/**
 * @brief Message table for german translations (\ref Language=1)
 */
static const char* messages_1[] = 
{
    "mksqlite Version " CONFIG_MKSQLITE_VERSION_STRING " " SVNREV ", ein MATLAB Interface zu SQLite\n"
    "(c) 2008-2017 by Martin Kortmann <mail@kortmann.de>\n"
    "                 Andreas Martin  <andimartin@users.sourceforge.net>\n"
    "basierend auf SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite verwendet:\n"
    " - DEELX perl kompatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " zur Datenkompression (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) Implementierung von Alexander Peslyak\n"
    "   \n",
    
/*  1*/    "ungueltiger Datenbankhandle",
/*  2*/    "Funktion nicht moeglich",
/*  3*/    "Verwendung: mksqlite([dbid,] Befehl [, Datenbankdatei])\n",
/*  4*/    "kein oder falsches Argument uebergeben",
/*  5*/    "mksqlite: Die noch geoeffneten Datenbanken wurden geschlossen",
/*  6*/    "getstring() kann keine neue Zeichenkette erstellen",
/*  7*/    "Open Befehl ohne Datenbanknamen",
/*  8*/    "kein freier Datenbankhandle verfuegbar",
/*  9*/    "Datenbank konnte nicht geoeffnet werden (ggf. Zugriffsrechte oder Existenz der Datenbank pruefen)",
/* 10*/    "Datenbank nicht geoeffnet",
/* 11*/    "ungueltiger query String (Semikolon?)",
/* 12*/    "kann Ausgabematrix nicht erstellen",
/* 13*/    "unbekannter SQLITE Datentyp",
/* 14*/    "busytimeout konnte nicht gesetzt werden",
/* 15*/    "konnte keinen eindeutigen Bezeichner fuer Feld %s bilden",
/* 16*/    "Argumentliste zu lang",
/* 17*/    "keine Argumentliste angegeben",
/* 18*/    "Fehler im Speichermanagement", 
/* 19*/    "nicht unterstuetzter Variablentyp",
/* 20*/    "unbekannter oder nicht unterstuetzter typisierter BLOB Header",
/* 21*/    "Fehler beim Identifizieren der Computerarchitektur",
/* 22*/    "BLOB wurde mit abweichender Computerarchitektur erstellt",
/* 23*/    "BLOB ist zu gross",
/* 24*/    "Fehler waehrend der Kompression aufgetreten",
/* 25*/    "unbekannte Komprimierung",
/* 26*/    "gewaehlter Kompressor erlaubt nur Datentyp 'double'",
/* 27*/    "gewaehlter Kompressor erlaubt nur positive Werte",
/* 28*/    "unbekannter Zugriffmodus (nur 'ro', 'rw' oder 'rwc' moeglich)",
/* 29*/    "unbekannter Threadingmodus (nur 'single', 'multi' oder 'serial' moeglich)",
/* 30*/    "die Datenbank kann nicht geschlossen werden",
/* 31*/    "nicht alle Datenbanken konnten geschlossen werden",
/* 32*/    "Feature wird von dieser Matlab Version nicht unterstuetzt",
/* 33*/    "DLL Erweiterungen fuer diese db aktiviert",
/* 34*/    "DLL Erweiterungen fuer diese db deaktiviert",
/* 35*/    "Einstellung fuer DLL Erweiterungen nicht moeglich",
/* 36*/    "Argumentliste zu kurz",
/* 37*/    "Argumentliste zu kurz (moeglicherweise eine Matrix statt Cell-Array uebergeben?)",
/* 38*/    "Feld fuer SQL Parameter '%s' fehlt",
/* 39*/    "numerischer Parameter erwartet",
/* 40*/    "einzelnes Argument vom Typ Cell nicht erlaubt, bei aktiviertem Streaming mit\nmehreren SQL Parametern oder ebenfalls aktiviertem Parameter Wrapping",
/* 41*/    "einzelnes Argument vom Typ Struct nicht erlaubt, bei aktiviertem Streaming mit\nmehreren SQL Parametern oder ebenfalls aktiviertem Parameter Wrapping",
/* 42*/    "aus dem Spaltennamen konnte kein gueltiger Feldname erzeugt werden",
/* 43*/    "fuer das Streamen von Variablen sind typisierte BLOBS erforderlich! Streaming ist ausgeschaltet",
/* 44*/    "Streaming wird von dieser MATLAB Version nicht unterstuetzt",
/* 45*/    "Rueckgabetyp ist ",
/* 46*/    "Datenbank ID wurde angegeben, ist fuer diesen Befehl jedoch ueberfluessig! ", 
/* 47*/    "Funktionshandle erwartet! ",
/* 48*/    "String Argument erwartet! ",
/* 49*/    "unzulaessiger rekursiver Funktionsaufruf! ",
/* 50*/    "ungueltige Funktion! ",
/* 51*/    "0 als dbid ist nur fuer die Befehle 'open' und 'close' erlaubt! ",
/* 52*/    "Interner Fehler! ",
/* 53*/    "Ausfuehrung abgebrochen (Ctrl+C)!",
};

/**
 * \brief Text representations
 * \sa RESULT_TYPES constants defined in config.h
 */
const char* STR_RESULT_TYPES[] = {
    "array of structs",   // RESULT_TYPE_ARRAYOFSTRUCTS
    "struct of arrays",   // RESULT_TYPE_STRUCTOFARRAYS  
    "matrix/cell array"   // RESULT_TYPE_MATRIX
};


/**
 * \brief Number of language in use
 * 
 * A number <0 means "uninitialized"
 */
static int Language = -1;


/**
 * \brief Message Tables
 */
static const char **messages[] = 
{
    messages_0,   /* English translations */
    messages_1    /* German translations  */
};


/** 
 * \brief Returns the translation for a defined message
 * \param[in] iMsgNr Message identifier (see \ref MSG_IDS "Message Identifier")
 * \returns Pointer to translation
 */
const char* getLocaleMsg( int iMsgNr )
{
    if(iMsgNr < 0)
    {
        switch( getLocale() )
        {
            case 1:
                return "Unbekanner Fehler!";
            default:
                return "Unspecified error!"; 
        }
    }
    else
    {
        return messages[Language][iMsgNr];
    } 
}


/** 
 * \brief Sets the current locale
 * \param[in] iLang number identifying current locale
 * \returns true on success
 */
bool setLocale( int iLang )
{
    if( iLang >=0 && iLang < sizeof(messages) / sizeof(messages[0]) )
    {
        Language = iLang;
        return true;
    } else {
        return false;
    }
}


/**
 * \brief Get current locale id
 */
int getLocale()
{
    return Language;
}

#endif