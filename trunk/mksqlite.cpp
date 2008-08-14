/*
 * MATLAB Schnittstelle zu SQLite
 */

#include <stdio.h>
#include <winsock.h>

#include <mex.h>
#include "sqlite3.h"

// Version
#define VERSION "1.0"

// Revision aus SVN
#include "svn_revision.h"

// #define Debug(s)    mexPrintf("mksqlite: "),mexPrintf(s), mexPrintf("\n")
#define Debug(s)

// Wir brauchen eine C-Schnittstelle, also 'extern "C"' das ganze
extern "C" void mexFunction(int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[]);

// Flag: Willkommensmeldung wurde ausgegeben
static bool FirstStart = false;
// Flag: NULL as NaN ausgeben
static bool NULLasNaN  = false;

static const double g_NaN = mxGetNaN();

#define MaxNumOfDbs 5
static sqlite3* g_dbs[MaxNumOfDbs] = { 0 };

/*
 * Eine simple String Klasse
 */

class SimpleString
{
public:

		 SimpleString ();
		 SimpleString (const char* src);
	 	 SimpleString (const SimpleString&);
virtual ~SimpleString ();

	operator const char* ()
		{
			return m_str;
		}

	SimpleString& operator = (const char*);
	SimpleString& operator = (const SimpleString&);

private:

	char* m_str;
	static const char* m_EmptyString;
};

const char * SimpleString::m_EmptyString = "";


SimpleString::SimpleString()
	:m_str (const_cast<char*> (m_EmptyString))
{
}

SimpleString::SimpleString(const char* src)
{
	if (! src || ! *src)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src);
		m_str = new char [len +1];
		memcpy (m_str, src, len +1);
	}
}

SimpleString::SimpleString (const SimpleString& src)
{
	if (src.m_str == m_EmptyString)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src.m_str);
		m_str = new char [len +1];
		memcpy (m_str, src.m_str, len +1);
	}
}

SimpleString& SimpleString::operator = (const char* src)
{
	if (m_str && m_str != m_EmptyString)
		delete [] m_str;

	if (! src || ! *src)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src);
		m_str = new char [len +1];
		memcpy (m_str, src, len +1);
	}

	return *this;
}

SimpleString& SimpleString::operator = (const SimpleString& src)
{
	if (&src != this)
	{
		if (m_str && m_str != m_EmptyString)
			delete [] m_str;

		if (src.m_str == m_EmptyString)
		{
			m_str = const_cast<char*> (m_EmptyString);
		}
		else
		{
			int     len = (int) strlen(src.m_str);
			m_str = new char [len +1];
			memcpy (m_str, src.m_str, len +1);
		}
	}

	return *this;
}

SimpleString::~SimpleString()
{
	if (m_str && m_str != m_EmptyString)
		delete [] m_str;
}

/*
 * Ein einzelner Wert mit Typinformation
 */
class Value
{
public:
    int          m_Type;

    SimpleString m_StringValue;
    double       m_NumericValue;
    
virtual    ~Value () {} 
};

/*
 * mehrere Werte...
 */
class Values
{
public:
	int     m_Count;
    Value*	m_Values;
    
    Values* m_NextValues;
    
         Values(int n) : m_Count(n), m_NextValues(0)
            { m_Values = new Value[n]; }
            
virtual ~Values() 
            { delete [] m_Values; }
};



/*
 * Die DllMain wurd dazu verwendet, um zu erkennen ob die Library von 
 * MATLAB beendet wird. Dann wird eine evtl. bestehende Verbndung
 * ebenfalls beendet.
 */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
        {
            bool dbsClosed = false;
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i])
                {
                    sqlite3_close(g_dbs[i]);
                    g_dbs[i] = 0;
                    dbsClosed = true;
                }
            }
            if (dbsClosed)
            {
                mexWarnMsgTxt ("mksqlite: Die noch geöffneten Datenbanken wurden geschlossen.\n");
            }
        }
		break;
	}
	return TRUE;
}

// Einen String von MATLAB holen
static char *getstring(const mxArray *a)
{
   int llen = mxGetM(a) * mxGetN(a) * sizeof(mxChar) + 1;
   char *c = (char *) mxCalloc(llen,sizeof(char));

   if (mxGetString(a,c,llen))
      mexErrMsgTxt("Can\'t copy string in getstring()");
   
   return c;
}

void mexFunction(int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[])
{
    {
        if (! FirstStart)
        {
            FirstStart = true;
            mexPrintf ("mksqlite Version " VERSION " " SVNREV ", ein MATLAB Interface zu SQLite\n"
                       "(c) 2008 by Martin Kortmann <email@kortmann.de>\n"
                       "basierend auf SQLite Version %s - http://www.sqlite.org\n\n", sqlite3_libversion());
        }
    }
    
    /*
     * Funktionsargumente überprüfen
     *
     * Wenn als erstes Argument eine Zahl übergeben wurde, so wird diese
     * als Datenbank-ID verwendet. Ist das erste Argument ein String, wird
     * Datenbank-ID 1 angenommen.
     * Alle weiteren Argumente müssen Strings sein.
     */

    int db_id = 0;
    int FirstArg = 0;
    int NumArgs = nrhs;
    
    if (nrhs >= 1 && mxIsNumeric(prhs[0]))
    {
        db_id = (int) *mxGetPr(prhs[0]);
        if (db_id < 0 || db_id > MaxNumOfDbs)
        {
            mexPrintf("ungültiger Datenbankhandle\n");
            mexErrMsgTxt("Funktion nicht möglich");
        }
        db_id --;
        FirstArg ++;
        NumArgs --;
    }
    
    // Alle Argumente müssen Strings sein
    bool isNotOK = false;
    int  i;
    
    for (i = FirstArg; i < nrhs; i++)
    {
        if (! mxIsChar(prhs[i]))
        {
            isNotOK = true;
            break;
        }
    }
    if (NumArgs < 1 || isNotOK)
    {
        mexPrintf("Verwendung: %s([dbid,] Befehl [, datenbankdatei])\n", mexFunctionName());
        mexErrMsgTxt("kein oder falsches Argument übergeben");
    }
    
    char *command = getstring(prhs[FirstArg]);
    SimpleString query(command);
    mxFree(command);
    
    Debug(query);
 
    if (! strcmp(query, "open"))
    {
        if (NumArgs != 2)
        {
            mexPrintf("Open ohne Datenbanknamen\n", mexFunctionName());
            mexErrMsgTxt("kein oder falsches Argument übergeben");
        }
        
        char* dbname = getstring(prhs[FirstArg +1]);

        // Wurde eine db-id angegeben, dann die entsprechende db schliessen
        if (db_id > 0 && g_dbs[db_id])
        {
            sqlite3_close(g_dbs[db_id]);
            g_dbs[db_id] = 0;
        }
        
        // bei db-id -1 neue id bestimmen
        if (db_id < 0)
        {
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i] == 0)
                {
                    db_id = i;
                    break;
                }
            }
        }
        if (db_id < 0)
        {
            plhs[0] = mxCreateScalarDouble((double) 0);
            mexPrintf("Kein freier Datenbankhandle verfügbar\n");
            mexErrMsgTxt("Funktion nicht möglich");
        }
        
        int rc = sqlite3_open(dbname, &g_dbs[db_id]);
        
        if (rc)
        {
            sqlite3_close(g_dbs[db_id]);
            
            mexPrintf("Datenbank konnte nicht geöffnet werden\n%s, ", sqlite3_errmsg(g_dbs[db_id]));

            g_dbs[db_id] = 0;
            plhs[0] = mxCreateScalarDouble((double) 0);
            
            mexErrMsgTxt("Funktion nicht möglich");
        }
        
        plhs[0] = mxCreateScalarDouble((double) db_id +1);
        mxFree(dbname);
    }
    else if (! strcmp(query, "close"))
    {
        if (db_id < 0)
        {
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i])
                {
                    sqlite3_close(g_dbs[i]);
                    g_dbs[i] = 0;
                }
            }
        }
        else
        {
            if (! g_dbs[db_id])
            {
                mexErrMsgTxt("Datenbank nicht geöffnet");
            }
            else
            {
                sqlite3_close(g_dbs[db_id]);
                g_dbs[db_id] = 0;
            }
        }
    }
    else
    {
        if (db_id < 0)
        {
            mexPrintf("Ungültiger Datenbankhandle\n");
            mexErrMsgTxt("Funktion nicht möglich");
        }
        
        if (!g_dbs[db_id])
        {
            mexErrMsgTxt("Datenbank nicht geöffnet");
        }

        if (! strcmpi(query, "show tables"))
        {
            query = "SELECT name as tablename FROM sqlite_master "
                    "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                    "UNION ALL "
                    "SELECT name as tablename FROM sqlite_temp_master "
                    "WHERE type IN ('table','view') "
                    "ORDER BY 1";
        }

        if (sqlite3_complete(query))
        {
            mexErrMsgTxt("ungültiger query String (Semicolon?)");
        }
        
        sqlite3_stmt *st;
        
        if (sqlite3_prepare_v2(g_dbs[db_id], query, -1, &st, 0))
        {
            if (st)
                sqlite3_finalize(st);
            
            mexErrMsgTxt(sqlite3_errmsg(g_dbs[db_id]));
        }

        int ncol = sqlite3_column_count(st);
        if (ncol > 0)
        {
            char **fieldnames = new char *[ncol];   // Die Feldnamen
            Values* allrows = 0;                    // Die Records
            Values* lastrow = 0;
            int rowcount = 0;
            
            for(int i=0; i<ncol; i++)
            {
                const char *cname = sqlite3_column_name(st, i);
                
                fieldnames[i] = new char [strlen(cname) +1];
                strcpy (fieldnames[i], cname);
                // ungültige Zeichen gegen _ ersetzen
                char *mk_c = fieldnames[i];
                while (*mk_c)
                {
                	if ((*mk_c == ' ') || (*mk_c == '*') || (*mk_c == '?'))
                    	*mk_c = '_';
                    mk_c++;
                }
            }
            
            // Daten einsammeln
            for(;;)
            {
                int step_res = sqlite3_step(st);

                if (step_res != SQLITE_ROW)
                    break;
               
                Values* RecordValues = new Values(ncol);
                
                Value *v = RecordValues->m_Values;
                for (int j = 0; j < ncol; j++, v++)
                {
                     int fieldtype = sqlite3_column_type(st,j);

                     v->m_Type = fieldtype;
                        
                     switch (fieldtype)
                     {
                         case SQLITE_NULL:      v->m_NumericValue = g_NaN;                                   break;
                         case SQLITE_INTEGER:	v->m_NumericValue = (double) sqlite3_column_int(st, j);      break;
                         case SQLITE_FLOAT:     v->m_NumericValue = (double) sqlite3_column_double(st, j);	 break;
                         case SQLITE_TEXT:      v->m_StringValue  = (const char*)sqlite3_column_text(st, j); break;
                                
                         default:	mexErrMsgTxt("unbek. SQLITE Datentyp");
                     }
                }
                if (! lastrow)
                {
                    allrows = lastrow = RecordValues;
                }
                else
                {
                    lastrow->m_NextValues = RecordValues;
                    lastrow = lastrow->m_NextValues;
                }
                rowcount ++;
            }
            
            sqlite3_finalize(st);

            if (rowcount == 0 || ! allrows)
            {
                if (!( plhs[0] = mxCreateDoubleMatrix(0,0,mxREAL) ))
                    mexErrMsgTxt("Kann Ausgabematrix nicht erstellen");
            }
            else
            {
                int ndims[2];
                
                ndims[0] = rowcount;
                ndims[1] = 1;
                
                if (!( plhs[0] = mxCreateStructArray (2, ndims, ncol, (const char**)fieldnames)))
                {
                    mexErrMsgTxt("Kann Ausgabematrix nicht erstellen");
                }
                
                lastrow = allrows;
                int i = 0;
                while(lastrow)
                {
                    Value* recordvalue = lastrow->m_Values;
                    
                    for (int j = 0; j < ncol; j++, recordvalue++)
                    {
                        if (recordvalue -> m_Type == SQLITE_TEXT)
                        {
                            mxArray* c = mxCreateString(recordvalue->m_StringValue);
                            mxSetFieldByNumber(plhs[0], i, j, c);
                        }
                        else if (recordvalue -> m_Type == SQLITE_NULL && !NULLasNaN)
                        {
                            mxArray* out_double = mxCreateDoubleMatrix(0,0,mxREAL);
                            mxSetFieldByNumber(plhs[0], i, j, out_double);
                        }
                        else
                        {
                            mxArray* out_double = mxCreateDoubleScalar(recordvalue->m_NumericValue);
                            mxSetFieldByNumber(plhs[0], i, j, out_double);
                        }
                    }
                    allrows = lastrow;
                    lastrow = lastrow->m_NextValues;
                    delete allrows;
                    i++;
                }
            }
            for(int i=0; i<ncol; i++)
                delete [] fieldnames[i];
            delete fieldnames;
        }
        else
        {
            int res = sqlite3_step(st);
            sqlite3_finalize(st);

            if (res != SQLITE_DONE)
            {
                mexErrMsgTxt(sqlite3_errmsg(g_dbs[db_id]));
            }            
        }
    }
    
    Debug("fertig");
}

/*
 *
 * Formatierungsanweisungen für den Editor vim
 *
 * vim:ts=4:ai:sw=4
 */
