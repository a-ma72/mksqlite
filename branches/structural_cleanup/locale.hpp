/**
 *  <!-- mksqlite: A MATLAB Interface to SQLite -->
 * 
 *  @file      locale.hpp
 *  @brief     (Error-)messages in english and german.
 *  @details   All text strings omitted by mksqlite are cumulated in this file
 *             for the case of further translations.
 *  @authors   Martin Kortmann <mail@kortmann.de>, 
 *             Andreas Martin  <andimartin@users.sourceforge.net>
 *  @version   2.0
 *  @date      2008-2015
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once

#include "config.h"
//#include "global.hpp"
#include "svn_revision.h" /* get the SVN revision number */
extern "C"
{
  #include "blosc/blosc.h"
}

/* Localization, declaration */

const char*   getLocaleMsg  ( int iMsgNr );
void          setLocale     ( int iLang );
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
 * its text is taken in a buffer \ref Err::m_err_msg.
 *
 * @{
 */
#define MSG_PURESTRING            -2
#define MSG_NOERROR               -1
#define MSG_HELLO                  0
#define MSG_INVALIDDBHANDLE        1
#define MSG_IMPOSSIBLE             2
#define MSG_USAGE                  3
#define MSG_INVALIDARG             4
#define MSG_CLOSINGFILES           5
#define MSG_CANTCOPYSTRING         6
#define MSG_NOOPENARG              7
#define MSG_NOFREESLOT             8
#define MSG_CANTOPEN               9
#define MSG_DBNOTOPEN             10
#define MSG_INVQUERY              11
#define MSG_CANTCREATEOUTPUT      12
#define MSG_UNKNWNDBTYPE          13
#define MSG_BUSYTIMEOUTFAIL       14
#define MSG_MSGUNIQUEWARN         15
#define MSG_UNEXPECTEDARG         16
#define MSG_MISSINGARGL           17
#define MSG_ERRMEMORY             18
#define MSG_UNSUPPVARTYPE         19
#define MSG_UNSUPPTBH             20   
#define MSG_ERRPLATFORMDETECT     21
#define MSG_WARNDIFFARCH          22
#define MSG_BLOBTOOBIG            23
#define MSG_ERRCOMPRESSION        24
#define MSG_UNKCOMPRESSOR         25
#define MSG_ERRCOMPRARG           26
#define MSG_ERRCOMPRLOGMINVALS    27
#define MSG_ERRUNKOPENMODE        28
#define MSG_ERRUNKTHREADMODE      29
#define MSG_ERRCANTCLOSE          30
#define MSG_ERRCLOSEDBS           31
#define MSG_ERRNOTSUPPORTED       32
#define MSG_EXTENSION_EN          33
#define MSG_EXTENSION_DIS         34
#define MSG_EXTENSION_FAIL        35
#define MSG_MISSINGARG            36
#define MSG_NUMARGEXPCT           37
#define MSG_SINGLECELLNOTALLOWED  38
#define MSG_ERRVARNAME            39
#define MSG_STREAMINGNEEDTYBLOBS  40
#define MSG_STREAMINGNOTSUPPORTED 41
#define MSG_RESULTTYPE            42
#define MSG_DBID_SUPFLOUS         43
/** @}  */


/**
 * \brief Helperclass for error message transport
 *
 */
class Err
{
    const char* m_err_msg;    ///< Holds pointer to message test if m_errId == MSG_PURESTRING
    char m_err_string[1024];  ///< Text buffer for non-const (generated) messages
    bool m_isPending;         ///< Message has still to be handled if this flag is set
    int  m_errId;             ///< Message ID (see \ref MSG_IDS "Message Identifiers")
    
public:
    
    /// Constructor
    Err() 
    { 
        clear(); 
    }
    
    /**
     * \brief Set error message to a constant string (without translation)
     *
     * \param[in] strMsg Pointer to constant message text
     */
    void set( const char* strMsg )
    {
         m_err_msg    = strMsg;
        *m_err_string = 0;
         m_isPending  = true;
         m_errId      = MSG_PURESTRING;
    }
    
    /** \brief Set error message to non-constant string (no translation)
     *
     * \param[in] strMsg Pointer to message text
     */
    void set( char* strMsg )
    {
        m_isPending   = true;
        m_errId       = MSG_PURESTRING;
        m_err_msg     = NULL;
        
        if( !strMsg ) 
        { 
            m_err_msg     = "";
            m_isPending   = false;
            m_errId       = MSG_NOERROR;
        }
        
        _snprintf( m_err_string, sizeof(m_err_string), "%s", strMsg );
    }
    
    /** 
     * \brief Set error message by identifier (translations available)
     *
     * \param[in] iMessageNr Message identifier (see \ref MSG_IDS "Message Identifiers")
     */
    void set( int iMessageNr )
    {
         set( ::getLocaleMsg( iMessageNr ) );
         if( iMessageNr == MSG_NOERROR )
         {
            m_isPending = false;
         }
         m_errId = iMessageNr;
    }
    
    /// Reset error message
    void clear()
    {
        set( MSG_NOERROR );
    }
    
    
    /// Get the current error message
    const char* get()
    {
        return m_err_msg ? m_err_msg : m_err_string;
    }
    
    /// Get the current message identifier
    int getErrId()
    {
        return m_errId;
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
    "mksqlite Version " MKSQLITE_VERSION_STRING " " SVNREV ", an interface from MATLAB to SQLite\n"
    "(c) 2008-2015 by Martin Kortmann <mail@kortmann.de>\n"
    "                 Andreas Martin  <andimartin@users.sourceforge.net>\n"
    "based on SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite uses further:\n"
    " - DEELX perl compatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " compression algorithm (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) implementation by Alexander Peslyak\n"
    "   \n",
    
    "invalid database handle",
    "function not possible",
    "usage: mksqlite([dbid,] command [, databasefile])\n",
    "no or wrong argument",
    "mksqlite: closing open databases",
    "can\'t copy string in getstring()",
    "open without database name",
    "no free database handle available",
    "cannot open database (check access privileges and existence of database)",
    "database not open",
    "invalid query string (semicolon?)",
    "cannot create output matrix",
    "unknown SQLITE data type",
    "cannot set busy timeout",
    "could not build unique field name for %s",
    "unexpected arguments passed",
    "missing argument list",
    "memory allocation error",
    "unsupported variable type",
    "unknown/unsupported typed blob header",
    "error while detecting the type of computer you are using",
    "BLOB stored on different type of computer",
    "BLOB exceeds maximum allowed size",
    "error while compressing data",
    "unknown compressor",
    "chosen compressor accepts 'double' type only",
    "chosen compressor accepts positive values only",
    "unknown open modus (only 'ro', 'rw' or 'rwc' accepted)",
    "unknown threading mode (only 'single', 'multi' or 'serial' accepted)",
    "cannot close connection",
    "not all connections could be closed",
    "this Matlab version doesn't support this feature",
    "extension loading enabled for this db",
    "extension loading disabled for this db",
    "failed to set extension loading feature",
    "missing argument",
    "numeric argument expected",
    "single cell argument not allowed when streaming enabled",
    "unable to create fieldname from column name",
    "streaming of variables needs typed BLOBs! Streaming is off",
    "streaming not supported in this MATLAB version",
    "Result type is ",
    "Database ID is given, but superflous! ",
};


/**
 * @brief Message table for german translations (\ref Language=1)
 */
static const char* messages_1[] = 
{
    "mksqlite Version " MKSQLITE_VERSION_STRING " " SVNREV ", ein MATLAB Interface zu SQLite\n"
    "(c) 2008-2015 by Martin Kortmann <mail@kortmann.de>\n"
    "                 Andreas Martin  <andimartin@users.sourceforge.net>\n"
    "basierend auf SQLite Version %s - http://www.sqlite.org\n"
    "mksqlite verwendet darüber hinaus:\n"
    " - DEELX perl kompatible regex engine Version " DEELX_VERSION_STRING " (Sswater@gmail.com)\n"
    " - BLOSC/LZ4 " BLOSC_VERSION_STRING " zur Datenkompression (Francesc Alted / Yann Collett) \n"
    " - MD5 Message-Digest Algorithm (RFC 1321) Implementierung von Alexander Peslyak\n"
    "   \n",
    
    "ungültiger Datenbankhandle",
    "Funktion nicht möglich",
    "Verwendung: mksqlite([dbid,] Befehl [, Datenbankdatei])\n",
    "kein oder falsches Argument übergeben",
    "mksqlite: Die noch geöffneten Datenbanken wurden geschlossen",
    "getstring() kann keine neue Zeichenkette erstellen",
    "Open Befehl ohne Datenbanknamen",
    "Kein freier Datenbankhandle verfügbar",
    "Datenbank konnte nicht geöffnet werden (ggf. Zugriffsrechte oder Existenz der Datenbank prüfen)",
    "Datenbank nicht geöffnet",
    "ungültiger query String (Semikolon?)",
    "Kann Ausgabematrix nicht erstellen",
    "unbekannter SQLITE Datentyp",
    "busytimeout konnte nicht gesetzt werden",
    "konnte keinen eindeutigen Bezeichner für Feld %s bilden",
    "Argumentliste zu lang",
    "keine Argumentliste angegeben",
    "Fehler bei Speichermanagement", 
    "Nicht unterstützter Variablentyp",
    "Unbekannter oder nicht unterstützter typisierter BLOB Header",
    "Fehler beim Identifizieren der Computerarchitektur",
    "BLOB wurde mit abweichender Computerarchitektur erstellt",
    "BLOB ist zu groß",
    "Fehler während der Kompression aufgetreten",
    "unbekannte Komprimierung",
    "gewählter Kompressor erlaubt nur Datentyp 'double'",
    "gewählter Kompressor erlaubt nur positive Werte",
    "unbekannter Zugriffmodus (nur 'ro', 'rw' oder 'rwc' möglich)",
    "unbekannter Threadingmodus (nur 'single', 'multi' oder 'serial' möglich)",
    "die Datenbank kann nicht geschlossen werden",
    "nicht alle Datenbanken konnten geschlossen werden",
    "Feature wird von dieser Matlab Version nicht unterstützt",
    "DLL Erweiterungen für diese db aktiviert",
    "DLL Erweiterungen für diese db deaktiviert",
    "Einstellung für DLL Erweiterungen nicht möglich",
    "Parameter fehlt",
    "numerischer Parameter erwartet",
    "einzelnes Argument vom Typ Cell ist nicht erlaubt, wenn das Streaming eingeschaltet ist",
    "aus dem Spaltennamen konnte kein gültiger Feldname erzeugt werden",
    "für das Streamen von Variablen sind typisierte BLOBS erforderlich! Streaming ist ausgeschaltet",
    "Streaming wird von dieser MATLAB Version nicht unterstützt",
    "Rückgabetyp ist ",
    "Datenbank ID wurde angegeben, ist für diesen Befehl jedoch überflüssig! ", 
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
    return (iMsgNr < 0) ? NULL : messages[Language][iMsgNr];
}


/** 
 * \brief Sets the current locale
 */
void setLocale( int iLang )
{
    if( iLang >=0 && iLang < sizeof(messages) )
    {
        Language = iLang;
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