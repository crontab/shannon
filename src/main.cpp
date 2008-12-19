
#include <stdio.h>

#include "charset.h"
#include "str.h"
#include "contain.h"
#include "except.h"
#include "source.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //


class EParser: public EMessage
{
protected:
    string filename;
    int linenum;
public:
    EParser(const string& ifilename, int ilinenum, const string& msg)
        : EMessage(msg), filename(ifilename), linenum(ilinenum)  { }
    virtual ~EParser() throw();
    virtual string what() const throw();
};


enum Token
{
    tokUndefined = -1,
    tokBlockBegin, tokBlockEnd, tokEnd, // these will depend on C-style vs. Python-style modes in the future
    tokEof,
    tokIdent, tokIntValue, tokStrValue,
    tokComma, tokPeriod, tokDiv
};


enum SyntaxMode { syntaxIndent, syntaxCurly };


class Parser
{
protected:
    InText* input;
    Stack<int> indentStack;

    void parseStringLiteral();
    void skipMultilineComment();

public:
    bool singleLineBlock; // if a: b = c
    Token token;
    string strValue;
    ularge intValue;
    
    Parser(const string& filename);
    ~Parser();
    
    Token next(bool expectBlock = false) throw(EParser, ESysError);

    void error(const string& msg) throw(EParser);
    void syntax(const string& msg) throw(EParser);
    
    int indentLevel()  { return indentStack.top(); }
};


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
    : input(new InFile(filename)),
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
    input->skipEol();
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
            return token = tokBlockEnd;
        }
        strValue = "<EOF>";
        return token = tokEof;
    }

    else if (input->getEol())
    {
        input->skipEol();
        if (singleLineBlock)
        {
            strValue = "<END>";
            singleLineBlock = false;
            return token = tokBlockEnd;
        }
        else
        {
            strValue = "<SEP>";
            return token = tokEnd;
        }
    }

    else if (c == '#')
    {
        input->get();
        if (input->preview() == '{')
            skipMultilineComment();
        else
            input->skipLine();
        goto restart;
    }

    else if (input->getNewLine())
    {
        // this is a new line, blanks are skipped, so we are at the first 
        // non-blank char:
        int newIndent = input->getIndent();
        int oldIndent = indentStack.top();
        if (newIndent > oldIndent)
        {
            strValue = "<BEGIN>";
            indentStack.push(newIndent);
            input->resetNewLine();
            return token = tokBlockBegin;
        }
        else if (newIndent < oldIndent)
        {
            strValue = "<END>";
            indentStack.pop();
            oldIndent = indentStack.top();
            if (newIndent > oldIndent)
                syntax("Unmatched un-indent");
            else if (newIndent == oldIndent)
            {
                input->resetNewLine();
                return token = tokBlockEnd;
            }
            else
                return token = tokBlockEnd;
        }
        // else: pass through to token analysis
    }


    // --- Handle ordinary tokens ---
    if (identFirst[c])  // identifier or keyword
    {
        strValue = input->get();
        strValue += input->token(identRest);
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
        case ';': strValue = "<SEP>"; return token = tokEnd;
        case ':':
            if (!expectBlock)
                syntax("Nested block expected");
            input->skip(wsChars);
            singleLineBlock = !input->getEol();
            return token = tokBlockBegin;
        case '/': return token = tokDiv;
        }
    }

    syntax("Illegal character '" + mkPrintable(c) + "'");

    return tokUndefined;
}


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
    }
} _atexit;



int main()
{
    Parser parser("tests/parser.txt");
    try
    {
        while (parser.next() != tokEof)
        {
            printf("%s ", parser.strValue.c_str());
            if (parser.token == tokBlockBegin || parser.token == tokBlockEnd || parser.token == tokEnd)
                printf("\n");
        }
    }
    catch (Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
    return 0;
}

