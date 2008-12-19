
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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
    throw ESysError(code);
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
        // NOTE: this list be kept in sorted order
        {"state", tokState},
        {"void", tokVoid},
        {NULL, tokUndefined}
    };



// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //


EParser::~EParser() throw() { }


string EParser::what() const throw()
{
    string s;
    if (!filename.empty())
        s = filename + '(' + itostring(linenum) + "): ";
    return s + EMessage::what();
}


const charset wsChars = "\t ";
const charset identFirst = "A-Za-z_";
const charset identRest = "0-9A-Za-z_";
const charset digits = "0-9";
const charset hexDigits = "0-9A-Fa-f";
const charset printableChars = "~20-~FF";


static string mkPrintable(char c)
{
    if (c == '\\')
        return string("\\\\");
    else if (c == '\'')
        return string("\\\'");
    else if (printableChars[c])
        return string(c);
    else
        return "\\x" + itostring(unsigned(c), 16);
}


Parser::Parser(const string& filename)
    : input(new InFile(filename)), blankLine(true),
      indentStack(), singleLineBlock(false),
      token(tokUndefined), strValue(), intValue(0)
{
    indentStack.push(0);
}


Parser::~Parser()
{
    delete input;
}


void Parser::error(const string& msg) throw(EParser)
{
    throw EParser(input->getFileName(), input->getLinenum(), msg);
}


void Parser::syntax(const string& msg) throw(EParser)
{
    error("Syntax error: " + msg);
}


void Parser::parseStringLiteral()
{
    static const charset stringChars = printableChars - charset("'\\");
    strValue.clear();
    while (true)
    {
        strValue += input->token(stringChars);
        if (input->getEof())
            syntax("Unexpected end of file in string literal");
        char c = input->get();
        if (input->isEolChar(c))
            syntax("Unexpected end of line in string literal");
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
                    syntax("Malformed hex sequence");
            }
            else
                strValue += c;
        }
        else
            syntax("Illegal character in string literal '" + mkPrintable(c) + "'");
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
            syntax("Illegal character in comments '" + mkPrintable(e) + "'");
    }
    input->skip(wsChars);
    if (!input->getEol())
        syntax("Multiline comments must end with a new line");
}


void Parser::skipSinglelineComment()
{
    static const charset commentChars = printableChars + wsChars;
    input->skip(commentChars);
    if (!input->getEol())
        syntax("Illegal character in comments '" + mkPrintable(input->preview()) + "'");
}


Token Parser::next(bool expectBlock) throw(EParser, ESysError)
{
restart:
    strValue.clear();

    input->skip(wsChars);

    char c = input->preview();

    // --- Handle EOF and EOL ---
    if (input->getEof())
    {
        // finalize all indents at end of file
        if (indentStack.size() > 1)
        {
            strValue = "<END>";
            indentStack.pop();
            return token = tokEnd;
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
            return token = tokEnd;
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
            strValue = "<BEGIN>";
            indentStack.push(newIndent);
            blankLine = false; // don't return to this branch again
            return token = tokBegin;
        }
        else if (newIndent < oldIndent) // unindent
        {
            strValue = "<END>";
            indentStack.pop();
            oldIndent = indentStack.top();
            if (newIndent > oldIndent)
                syntax("Unmatched un-indent");
            else if (newIndent == oldIndent)
            {
                blankLine = false; // don't return to this branch again
                return token = tokEnd;
            }
            else
                // by keeping blankLine = true we force to return to this branch
                // next time so that proper number of <END>s are generated
                return token = tokEnd;
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
        strValue = input->token(digits);
        bool e, o;
        intValue = stringtou(strValue.c_str(), &e, &o, 10);
        if (e)
            error("'" + strValue + "' is not a valid number");
        if (o)
            error("Numeric overflow (" + strValue + ")");
        return token = tokIntValue;
    }
    
    else  // special chars
    {
        strValue = input->get();
        switch (c)
        {
        case ',': return token = tokComma;
        case '.': return token = tokPeriod;
        case '\'': parseStringLiteral(); return token = tokStrValue;
        case ';': strValue = "<SEP>"; return token = tokSep;
        case ':':
            if (!expectBlock)
                syntax("Nested block expected");
            input->skip(wsChars);
            singleLineBlock = !input->getEol();
            return token = tokBegin;
        case '/': return token = tokDiv;
        }
    }

    syntax("Illegal character '" + mkPrintable(c) + "'");

    return tokUndefined;
}

