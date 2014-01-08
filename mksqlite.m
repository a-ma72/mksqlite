%MKSQLITE Eine MATLAB Schnittstelle zu SQLite
%  SQLite ist eine Embedded SQL Engine, welche ohne Server SQL Datenbanken
%  innerhalb von Dateien verwalten kann. MKSQLITE bietet die Schnittstelle
%  zu dieser SQL Datenbank.
%
% Genereller Aufruf:
%  dbid = mksqlite([dbid, ] SQLBefehl [, Argument])
%    Der Parameter dbid ist optional und wird nur dann benötigt, wenn mit
%    mehreren Datenbanken gleichzeitig gearbeitet werden soll. Wird dbid
%    weggelassen, so wird automatisch die Datenbank Nr. 1 verwendet.
%
% Funktionsaufrufe:
%  mksqlite('open', 'datenbankdatei')
% oder
%  dbid = mksqlite(0, 'open', 'datenbankdatei')
% Öffnet die Datenbankdatei mit dem Dateinamen "datenbankdatei". Wenn eine
% solche Datei nicht existiert wird sie angelegt.
% Wenn eine dbid angegeben wird und diese sich auf eine bereits geöffnete
% Datenbank bezieht, so wird diese vor Befehlsausführung geschlossen. Bei
% Angabe der dbid 0 wird die nächste freie dbid zurück geliefert.
%
%  mksqlite('close')
% oder
%  mksqlite(dbid, 'close')
% oder
%  mksqlite(0, 'close')
% Schliesst eine Datenbankdatei. Bei Angabe einer dbid wird diese Datenbank
% geschlossen. Bei Angabe der dbid 0 werden alle offenen Datenbanken
% geschlossen.
%
%  mksqlite('version mex')                 (1
% oder
%  version = mksqlite('version mex')       (2
% Gibt die Version von mksqlite in der Ausgabe (1), oder als String (2) zurück.
%
%
%  mksqlite('version sql')                 (1
% oder
%  version = mksqlite('version sql')       (2
% Gibt die Version der verwendeten SQLite Engine in der Ausgabe (1),
% oder als String (2) zurück.
%
%  mksqlite('SQL-Befehl')
% oder
%  mksqlite(dbid, 'SQL-Befehl')
% Führt SQL-Befehl aus.
%
% Beispiel:
%  mksqlite('open', 'testdb.db3');
%  result = mksqlite('select * from testtable');
%  mksqlite('close');
% Liest alle Felder der Tabelle "testtable" in der Datenbank "testdb.db3"
% in die Variable "result" ein.
%
% Beispiel:
%  mksqlite('open', 'testdb.db3')
%  mksqlite('show tables')
%  mksqlite('close')
% Zeigt alle Tabellen in der Datenbank "testdb.db3" an.
%
% =====================================================================
% Parameter binding:
% Die SQL Syntax erlaubt die Verwendung von Parametern, die vorerst nur
% durch Platzhalter gekennzeichnet und durch nachträgliche Argumente
% mit Inhalten gefüllt werden.
% Erlaubte Platzhalter in SQLlite sind: ?, ?NNN, :NNN, $NAME, @NAME
% Ein Platzhalter kann nur für einen Wert (value) stehen, nicht für
% einen Befehl, Spaltennamen, Tabelle, usw.
%
% Beispiel:
%  mksqlite( 'insert vorname, nachname, ort into Adressbuch values (?,?,?)', ...
%            'Gunther', 'Meyer', 'München' );
%
% Statt einer Auflistung von Argumenten, darf auch ein CellArray übergeben
% werden, dass die Argumente enthält.
% Werden weniger Argumente übergeben als benötigt, werden die verbleibenden
% Parameter mit NULL belegt. Werden mehr Argumente übergeben als
% benötigt, bricht die Funktion mit einer Fehlermeldung ab.
% Ein Argument darf ein realer numerischer Wert (Skalar oder Array)
% oder ein String sein. Nichtskalare Werte werden als Vektor vom SQL Datentyp
% BLOB (uint8) verarbeitet. ( BLOB = (B)inary (L)arge (OB)ject) )
%
% Beispiel:
%  data = rand(10,15);
%  mksqlite( 'insert data into MyTable values (?)', data );
%  query = mksqlite( 'select data from MyTable' );
%  data_sql = typecast( query(1).data, 'double' );
%  data_sql = reshape( data_sql, 10, 15 );
%
% BLOBs werden immer als Vektor aus uint8 Werten in der Datenbank abgelegt.
% Um wieder ursprüngliche Datenformate (z.B. double) und Dimensionen
% der Matrix zu erhalten muss explizit typecast() und reshape() aufgerufen werden.
% (Siehe hierzu auch das Beispiel "sqlite_test_bind.m")
% Wahlweise kann diese Information (Typisierung) im BLOB hinterlegt werden.
% Die geschilderte Nachbearbeitung ist dann zwar nicht mehr nötig, u.U. ist die
% Datenbank jedoch nicht mehr kompatibel zu anderer Software!
% Die Typisierung kann mit folgendem Befehl aktiviert/deaktiviert werden:
%
%   mksqlite( 'typedBLOBs', 1 ); % Aktivieren
%   mksqlite( 'typedBLOBs', 0 ); % Deaktivieren
%
% (Siehe auch Beispiel "sqlite_test_bind_typed.m")
% Typisiert werden nur numerische Arrays und Vektoren. Strukturen, Cellarrays
% und komplexe Daten sind nicht zulässig und müssen vorher konvertiert werden.
%
%
% Die Daten in einem BLOB werden entweder unkomprimiert (Standard) oder komprimiert
% abgelegt. Eine automatische Komprimierung der Daten ist nur für typisierte BLOBs
% (s.o.) zulässig und muss zuvor aktiviert werden:
%
%   mksqlite( 'compression', 'lz4', 9 ); % Maximale Kompression aktivieren (0=aus)
%
% (Siehe auch Beispiel "sqlite_test_bind_typed_compressed.m" und
% "sqlite_test_md5_and_packaging.m")
% Zur Komprimierung wird BLOSC (http://blosc.pytables.org/trac) verwendet.
% Nach dem Komprimieren der Daten werden sie erneut entpackt und mit dem
% Original verglichen. Weichen die Daten ab, wird eine entsprechende Fehlermeldung
% ausgegeben. Wenn diese Funktionalität nicht gewünscht ist (Daten werden ungeprüft
% gespeichert), kann sie auch deaktiviert werden:
%
%   mksqlite( 'compression_check', 0 ); % Check deaktivieren (1=aktivieren)
%
% Kompatibilität:
% Komprimiert abgelegte BLOBs können Sie nicht mit einer älteren Version von
% mksqlite abrufen, es kommt dann zu einer Fehlermeldung. Unkomprimierte BLOBs
% hingegen können auch mit der Vorgängerversion abgerufen werden.
% Mit der Vorgängerversion gespeicherte BLOBs können Sie natürlich auch mit dieser
% Version abrufen.
%
% Anmerkungen zur Kompressionsrate:
% Die erzielbaren Kompressionsraten hängen stark vom Inhalt der Variablen ab.
% Obwohl BLOSC für die Verwendung von Zahldatenformaten ausgelegt ist, ist die
% Kompressionsrate für randomisierte Zahlen (double) schlecht (~95%).
% Wenn viele gleiche Zahlen, z.B. durch Quantisierung, vorliegen wird die
% Kompressionsrate deutlich besser ausfallen...
%
% =======================================================================
%
% Extra SQL Funktionen:
% mksqlite bietet zusätzliche SQL Funktionen neben der bekannten "core functions"
% wie replace,trim,abs,round,...
% In dieser Version werden 7 weitere Funktionen angeboten:
%   * pow(x,y):
%     Berechnet x potenziert um den Exponenten y. Ist der Zahlenwert des Ergebnisses
%     nicht darstellbar ist der Rückgabewert NULL.
%   * regex(str,pattern):
%     Ermittelt den ersten Teilstring von str, der dem Regulären Ausdruck pattern
%     entspricht.
%   * regex(str,pattern,repstr):
%     Ermittelt den ersten Teilstring von str, der dem Regulären Ausdruck pattern
%     entspricht. Der Rückgabewert wird jedoch durch die neue Zusammensetzungsvorschrift
%     repstr gebildet.
%     (mksqlite verwendet die perl kompatible regex engine "DEELX".
%     Weiterführende Informationen siehe www.regexlab.com oder wikipedia)
%   * md5(x):
%     Es wird der MD5 Hashing Wert von x berechnet und ausgegeben.
%   * bdcpacktime(x):
%     Berechnet die erforderliche Zeit um x mit dem aktuellen Kompressor
%     und der eingestellten Konpression zu packen (Nettozeit)
%   * bdcunpacktime(x):
%     Das Äquivalent zu bdcpacktime(x).
%   * bdcratio(x):
%     Berechnet den Kompressionsfaktor, bezogen auf x und die derzeit
%     eingestellte Kompression.
%
% Die Verwendung von regex in Kombination mit parametrischen Parametern bieten eine
% besonders effiziente Möglichkeit komplexe Abfragen auf Textinhalte anzuwenden.
% Beispiel:
%   mksqlite( [ 'SELECT REGEX(field1,"[FMA][XYZ]MR[VH][RL]") AS re_field FROM Table ', ...
%               'WHERE REGEX(?,?,?) NOT NULL' ], 'field2', '(\\d{5})_(.*)', '$1' );
%
% (siehe auch test_regex.m für weitere Beispiele...)
%
%
% (c) 2008 by Martin Kortmann <mail@kortmann.de>
%

