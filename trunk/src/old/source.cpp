
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "source.h"
#include "bsearch.h"


InText::InText()
    : buffer(NULL), bufsize(0), bufpos(0), linenum(0),
      column(0), eof(false), tabsize(DEFAULT_TAB_SIZE)
{
}


InText::~InText()  { }


void InText::error(int code) throw(ESysError)
{
    eof = true;
    throw ESysError(code, getFileName());
}


char InText::preview()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    return buffer[bufpos];
}


char InText::get()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    return buffer[bufpos++];
}


bool InText::getIf(char c)
{
    if (preview() == c)
    {
        get();
        return true;
    }
    return false;
}


void InText::doSkipEol()
{
    linenum++;
    column = 0;
}


void InText::skipEol()
{
    char c = preview();
    if (c == '\r')
    {
        get();
        c = preview();
    }
    if (c == '\n')
    {
        get();
        doSkipEol();
    }
}


bool InText::getEol()
{
    char c = preview();
    return eof || isEolChar(c);
}


void InText::token(const charset& chars, string& result, bool noresult)
{
    do
    {
        if (bufpos == bufsize)
            validateBuffer();
        if (eof)
            return;
        const char* b = buffer + bufpos;
        register const char* p = b;
        register const char* e = buffer + bufsize;
        while (p < e && chars[*p])
        {
            switch (*p)
            {
                case '\t': column = ((column / tabsize) + 1) * tabsize; break;
                case '\n': doSkipEol(); break;
                default: column++; break;
            }
            p++;
        }
        bufpos += p - b;
        if (!noresult && p > b)
            result.append(b, p - b);
    }
    while (bufpos == bufsize);
}


string InText::token(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, false);
    return result;
}


void InText::skip(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, true);
}


void InText::skipLine()
{
    static const charset noneol = ~charset("\r\n");
    skip(noneol);
    skipEol();
}


InFile::InFile(const string& ifilename)
    : InText(), filename(ifilename), fd(-1)
{
    buffer = (char*)memalloc(INFILE_BUFSIZE);
}


InFile::~InFile()
{
    if (fd >= 0)
    {
        close(fd);
        eof = true;
        fd = -1;
    }
    memfree(buffer);
    buffer = NULL;
}


void InFile::validateBuffer()
{
    if (!eof && fd < 0)
    {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0)
            error(errno);
        linenum = 1;
    }
    if (!eof && bufpos == bufsize)
    {
        int result = read(fd, buffer, INFILE_BUFSIZE);
        if (result < 0)
            error(errno);
        bufpos = 0;
        bufsize = result;
        eof = result == 0;
    }
}


string InFile::getFileName()
{
    return filename;
}


// --- KEYWORDS ------------------------------------------------------------ //


class Keywords
{
    struct kwinfo { const char* kw; Token token; };
    static kwinfo keywords[];

    int count;

public:
    Keywords()
    {
        for (kwinfo* k = keywords; k->kw != NULL; k++)
        {
            if (count > 0)
                if (strcmp(k->kw, (k - 1)->kw) <= 0)
                    fatal(CRIT_FIRST + 40, "Keyword verification failed");
            count++;
        }
    }
    
    int compare(int index, const char* b) const
    {
        return strcmp(keywords[index].kw, b);
    }

    Token find(const char* s)
    {
        int index;
        if (bsearch<Keywords, const char*> (*this, s, count, index))
            return keywords[index].token;
        else
            return tokUndefined;
    }

} keywords;


Keywords::kwinfo Keywords::keywords[] =
    {
        // NOTE: this list must be kept in sorted order
        {"and", tokAnd},
        {"as", tokAs},
        {"assert", tokAssert},
        {"begin", tokBegin},
        {"break", tokBreak},
        {"case", tokCase},
        {"const", tokConst},
        {"continue", tokContinue},
        {"def", tokDef},
        {"echo", tokEcho},
        {"elif", tokElif},
        {"else", tokElse},
        {"enum", tokEnum},
        {"false", tokFalse},
        {"if", tokIf},
        {"in", tokIn},
        {"is", tokIs},
        {"mod", tokMod},
        {"module", tokModule},
        {"not", tokNot},
        {"null", tokNull},
        {"or", tokOr},
        {"return", tokReturn},
        {"shl", tokShl},
        {"shr", tokShr},
        {"sizeof", tokSizeOf},
        {"true", tokTrue},
        {"typeof", tokTypeOf},
        {"var", tokVar},
        {"while", tokWhile},
        {"xor", tokXor},
        {NULL, tokUndefined}
    };



// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //


static string parserErrorStr(const string& filename, int linenum, const string& msg)
{
    string s;
    if (!filename.empty())
        s = filename + '(' + itostring(linenum) + "): ";
    return s + msg;
}


EParser::EParser(const string& ifilename, int ilinenum, const string& msg)
    : Exception(parserErrorStr(ifilename, ilinenum, msg))  { }


ENotFound::ENotFound(const string& ifilename, int ilinenum, const string& ientry)
    : EParser(ifilename, ilinenum, "Unknown identifier '" + ientry + '\'')  { }


const charset wsChars = "\t ";
const charset identFirst = "A-Za-z_";
const charset identRest = "0-9A-Za-z_";
const charset digits = "0-9";
const charset hexDigits = "0-9A-Fa-f";
const charset printableChars = "~20-~FF";


string mkPrintable(char c)
{
    if (c == '\\')
        return string("\\\\");
    else if (c == '\'')
        return string("\\\'");
    else if (printableChars[c])
        return string(c);
    else
        return "\\x" + itostring(unsigned(c), 16, 2, '0');
}


string mkPrintable(const string& s)
{
    const char* p = s.c_bytes();
    const char* e = p + s.size();
    string result;
    for (; p < e; p++)
        result += mkPrintable(*p);
    return result;
}


Parser::Parser(const string& filename)
    : input(new InFile(filename)), blankLine(true),
      indentStack(), linenum(0),
      singleLineBlock(false), token(tokUndefined), strValue(),
      intValue(0), largeValue(0)
{
    indentStack.push(0);
}


Parser::~Parser()
{
    delete input;
}


void Parser::error(const string& msg)
    { throw EParser(input->getFileName(), getLineNum(), "Error: " + msg); }

void Parser::errorWithLoc(const string& msg)
    { error(msg + errorLocation()); }

void Parser::error(const char* msg)
    { error(string(msg)); }

void Parser::errorWithLoc(const char* msg)
    { errorWithLoc(string(msg)); }

void Parser::errorNotFound(const string& ident)
    { throw ENotFound(input->getFileName(), getLineNum(), ident); }




void Parser::parseStringLiteral()
{
    static const charset stringChars = printableChars - charset("'\\");
    strValue.clear();
    while (true)
    {
        strValue += input->token(stringChars);
        if (input->getEof())
            error("Unexpected end of file in string literal");
        char c = input->get();
        if (input->isEolChar(c))
            error("Unexpected end of line in string literal");
        if (c == '\'')
            return;
        else if (c == '\\')
        {
            c = input->get();
            if (c == 't')
                strValue += '\t';
            else if (c == 'r')
                strValue += '\r';
            else if (c == 'n')
                strValue += '\n';
            else if (c == 'x')
            {
                string s;
                if (hexDigits[input->preview()])
                {
                    s += input->get();
                    if (hexDigits[input->preview()])
                        s += input->get();
                    bool e, o;
                    ularge value = stringtou(s.c_str(), &e, &o, 16);
                    strValue += char(value);
                }
                else
                    error("Malformed hex sequence");
            }
            else
                strValue += c;
        }
        else
            error("Illegal character in string literal '" + mkPrintable(c) + "'");
    }
}


void Parser::skipMultilineComment()
{
    static const charset commentChars = (printableChars - '}') + wsChars;
    while (true)
    {
        input->skip(commentChars);
        if (input->getEol())
        {
            if (input->getEof())
                error("Unexpected end of file in comments");
            input->skipEol();
            linenum = input->getLineNum();
            continue;
        }
        char e = input->get();
        if (e == '}')
        {
            if (input->preview() == '#')
            {
                input->get();
                break;
            }
        }
        else
            error("Illegal character in comments '" + mkPrintable(e) + "'");
    }
    input->skip(wsChars);
    if (!input->getEol())
        error("Multiline comments must end with a new line");
}


void Parser::skipSinglelineComment()
{
    static const charset commentChars = printableChars + wsChars;
    input->skip(commentChars);
    if (!input->getEol())
        error("Illegal character in comments '" + mkPrintable(input->preview()) + "'");
}


Token Parser::next()
{
restart:
    strValue.clear();
    intValue = 0;
    largeValue = 0;

    // Deferred linenum update; this helps to point to a better location
    // in error messages.
    linenum = input->getLineNum();

    input->skip(wsChars);

    char c = input->preview();

    // --- Handle EOF and EOL ---
    if (input->getEof())
    {
        // finalize all indents at end of file
        if (indentStack.size() > 0)
        {
            strValue = "<END>";
            indentStack.pop();
            return token = tokBlockEnd;
        }
        strValue = "<EOF>";
        return token = tokEof;
    }

    else if (input->getEol())
    {
        input->skipEol();
        if (blankLine)
            goto restart;
        blankLine = true; // start from a new line
        if (singleLineBlock)
        {
            strValue = "<END>";
            singleLineBlock = false;
            return token = tokBlockEnd;
        }
        else
        {
            strValue = "<SEP>";
            return token = tokSep;
        }
    }

    else if (c == '#')  // single- or multiline comments
    {
        input->get();
        // both functions stop exactly at EOL
        if (input->preview() == '{')
            skipMultilineComment();
        else
            skipSinglelineComment();
        // if the comment started on a non-blank line, we have to generate <SEP>,
        // so we simply preserve blankLine
        goto restart;
    }

    else if (blankLine)
    {
        // this is a new line, blanks are skipped, so we are at the first 
        // non-blank, non-comment char:
        
        int newIndent = input->getColumn();
        int oldIndent = indentStack.top();
        if (newIndent > oldIndent)
        {
            strValue = "<INDENT>";
            indentStack.push(newIndent);
            blankLine = false; // don't return to this branch again
            return token = tokIndent;
        }
        else if (newIndent < oldIndent) // unindent
        {
            strValue = "<END>";
            indentStack.pop();
            oldIndent = indentStack.top();
            if (newIndent > oldIndent)
                error("Unmatched un-indent");
            else if (newIndent == oldIndent)
            {
                blankLine = false; // don't return to this branch again
                return token = tokBlockEnd;
            }
            else
                // by keeping blankLine = true we force to return to this branch
                // next time so that proper number of <END>s are generated
                return token = tokBlockEnd;
        }
        // else: same indent level, so pass through to token analysis
    }

    blankLine = false;

    // --- Handle ordinary tokens ---
    if (identFirst[c])  // identifier or keyword
    {
        strValue = input->get();
        strValue += input->token(identRest);
        Token tok = keywords.find(strValue.c_str());
        if (tok != tokUndefined)
            return token = tok;
        else
            return token = tokIdent;
    }
    
    else if (digits[c])  // numeric
    {
        bool e, o;

        strValue = input->token(identRest);
        string s = strValue;
        bool isHex = s.size() >= 2 && s[0] == '0' && s[1] == 'x';
        if (isHex)
            s.del(0, 2);
        bool isLarge = !s.empty() && s[s.size() - 1] == 'L';
        if (isLarge)
            s.resize(s.size() - 1);

        ularge v = stringtou(s.c_str(), &e, &o, isHex ? 16 : 10);
        if (e)
            error("'" + strValue + "' is not a valid number");
        if (o || (isLarge && v > LLONG_MAX) || (!isLarge && v > INT_MAX))
            error("Numeric overflow (" + strValue + ")");

        if (isLarge)
            largeValue = v;
        else
            intValue = v;
        
        return token = isLarge ? tokLargeValue : tokIntValue;
    }
    
    else  // special chars
    {
        strValue = input->get();
        switch (c)
        {
        case '\\':
            input->skip(wsChars);
            if (!input->getEol())
                error("New line expected after '\\'");
            input->skipEol();
            goto restart;
        case ',': return token = tokComma;
        case '.': return token = (input->getIf('.') ? tokRange : tokPeriod);
        case '\'': parseStringLiteral(); return token = tokStrValue;
        case ';': strValue = "<SEP>"; return token = tokSep;
        case ':':
            input->skip(wsChars);
            singleLineBlock = !input->getEol();
            if (singleLineBlock)
                return token = tokBlockBegin;
            else
            {
                if (next() != tokSep || next() != tokIndent)
                    error("Indented block expected after the colon");
                return token = tokBlockBegin;
            }
        case '+': return token = tokPlus;
        case '-': return token = tokMinus;
        case '/': return token = tokDiv;
        case '*': return token = tokMul;
        case '[': return token = tokLSquare;
        case ']': return token = tokRSquare;
        case '(': return token = tokLParen;
        case ')': return token = tokRParen;
        // case '{': return token = tokLCurly;
        // case '}': return token = tokRCurly;
        case '<': return token = (input->getIf('=') ? tokLessEq : tokLAngle);
        case '>': return token = (input->getIf('=') ? tokGreaterEq : tokRAngle);
        case '=': return token = (input->getIf('=') ? tokEqual : tokAssign);
        case '!': return token = (input->getIf('=') ? tokNotEq : tokExclam);
        case '|': return token = tokCat; break;
        }
    }

    error("Illegal character '" + mkPrintable(c) + "'");

    return tokUndefined;
}


string Parser::getIdent()
{
    if (token != tokIdent)
        errorWithLoc("Identifier expected");
    string result = strValue;
    next();
    return result;
}


string Parser::errorLocation() const
{
    string msg;
    if (!strValue.empty())
    {
        string s = strValue;
        if (s.size() > 30)
            s = s.copy(0, 30) + "...";
        msg += " near '" + s + "'";
    }
    return msg;
}


void Parser::skipSep()
{
    if (token == tokBlockEnd || token == tokEof)
        return;
    if (token != tokSep)
        errorWithLoc("End of statement expected");
    next();
}


void Parser::skip(Token tok, const char* errName)
{
    if (token != tok)
        errorWithLoc("'" + string(errName) + "' expected");
    next();
}


void Parser::skipBlockBegin()
    { skip(tokBlockBegin, "<BEGIN>"); }


void Parser::skipBlockEnd()
    { skip(tokBlockEnd, "<END>"); }


string extractFileName(string filepath)
{
    const char* p = filepath.c_str();
    const char* b = strrchr(p, '/');
    if (b == NULL)
        b = p;
    else
        b++;
    const char* e = strchr(b, '.');
    if (e == NULL)
        e = b + strlen(b);
    return filepath.copy(b - p, e - b);
}

