
#include "common.h"
#include "bsearch.h"
#include "source.h"

#include <fcntl.h>
#include <errno.h>


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
                    fatal(0x2040, "Keyword verification failed");
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
        {"finally", tokFinally},
        {"if", tokIf},
        {"in", tokIn},
        {"is", tokIs},
        {"mod", tokMod},
        {"module", tokModule},
        {"not", tokNot},
        {"or", tokOr},
        {"return", tokReturn},
        {"shl", tokShl},
        {"shr", tokShr},
        {"sizeof", tokSizeOf},
        {"typeof", tokTypeOf},
        {"while", tokWhile},
        {"xor", tokXor},
        {NULL, tokUndefined}
    };



// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //


static str parserErrorStr(const str& filename, int linenum, const str& msg)
{
    str s;
    if (!filename.empty())
        s = filename + '(' + to_string(linenum) + "): ";
    return s + msg;
}


EParser::EParser(const str& ifilename, int ilinenum, const str& msg)
    : emessage(parserErrorStr(ifilename, ilinenum, msg))  { }


ENotFound::ENotFound(const str& ifilename, int ilinenum, const str& ientry)
    : EParser(ifilename, ilinenum, "Unknown identifier '" + ientry + '\'')  { }


const charset wsChars = "\t ";
const charset identFirst = "A-Za-z_";
const charset identRest = "0-9A-Za-z_";
const charset digits = "0-9";
const charset hexDigits = "0-9A-Fa-f";
const charset printableChars = "~20-~FF";


str mkPrintable(char c)
{
    if (c == '\\')
        return str("\\\\");
    else if (c == '\'')
        return str("\\\'");
    else if (printableChars[c])
        return str(1, c);
    else
        return "\\x" + to_string(unsigned(c), 16, 2, '0');
}


str mkPrintable(const str& s)
{
    const char* p = s.data();
    const char* e = p + s.size();
    str result;
    for (; p < e; p++)
        result += mkPrintable(*p);
    return result;
}


Parser::Parser(const str& fn, fifo_intf* input)
    : fileName(fn), input(input), newLine(true),
      indentStack(), linenum(1), indent(0),
      singleLineBlock(false), curlyLevel(0),
      token(tokUndefined), strValue(),
      intValue(0)
{
    indentStack.push(0);
}


Parser::~Parser()
{
}


void Parser::error(const str& msg)
    { throw EParser(getFileName(), getLineNum(), "Error: " + msg); }

void Parser::errorWithLoc(const str& msg)
    { error(msg + errorLocation()); }

void Parser::error(const char* msg)
    { error(str(msg)); }

void Parser::errorWithLoc(const char* msg)
    { errorWithLoc(str(msg)); }

void Parser::errorNotFound(const str& ident)
    { throw ENotFound(getFileName(), getLineNum(), ident); }



void Parser::parseStringLiteral()
{
    static const charset stringChars = printableChars - charset("'\\");
    strValue.clear();
    while (true)
    {
        strValue += input->token(stringChars);
        if (input->eof())
            error("Unexpected end of file in string literal");
        char c = input->get();
        if (input->is_eol_char(c))
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
                str s;
                if (hexDigits[input->preview()])
                {
                    s += input->get();
                    if (hexDigits[input->preview()])
                        s += input->get();
                    bool e, o;
                    uinteger value = from_string(s.c_str(), &e, &o, 16);
                    strValue += char(value);
                }
                else
                    error("Bad hex sequence");
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
        if (input->eol())
        {
            if (input->eof())
                error("Unexpected end of file in comments");
            skipEol();
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
    if (!input->eol())
        error("Multiline comments must end with a new line");
}


void Parser::skipSinglelineComment()
{
    static const charset commentChars = printableChars + wsChars;
    input->skip(commentChars);
    if (!input->eol())
        error("Illegal character in comments '" + mkPrintable(input->preview()) + "'");
}


void Parser::skipEol()
{
    input->skip_eol();
    linenum++;
    if (!input->eof())
        indent = input->skip_indent();
}


// TODO: test curly mode

Token Parser::next()
{
restart:
    strValue.clear();
    intValue = 0;

    input->skip(wsChars);

    int c = input->preview();

    // --- Handle EOF and EOL ---
    if (c == -1)
    {
        // Finalize all indents at end of file; a file itself is a block so
        // the compiler needs a block-end here.
        if (curlyLevel > 0)
            error("Unbalanced curly brackets in file");
        else if (indentStack.size() > 0)
        {
            strValue = "<END>";
            indentStack.pop();
            return token = tokBlockEnd;
        }
        strValue = "<EOF>";
        return token = tokEof;
    }

    else if (input->eol())
    {
        skipEol();
        if (newLine)
            goto restart;       // Ignore blank lines, no matter what indent.
        newLine = true;         // Start from a new line.
        if (singleLineBlock)    // Only possible in Python mode
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
        // so we simply preserve newLine
        goto restart;
    }

    else if (curlyLevel == 0 && newLine)
    {
        // This is a new line, blanks are skipped, so we are at the first 
        // non-blank, non-comment char:
        int newIndent = getIndent();
        int oldIndent = indentStack.top();
        if (newIndent > oldIndent)
        {
            strValue = "<INDENT>";
            indentStack.push(newIndent);
            newLine = false; // don't return to this branch again
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
                newLine = false; // don't return to this branch again
                return token = tokBlockEnd;
            }
            else
                // by keeping newLine = true we force to return to this branch
                // next time so that proper number of <END>s are generated
                return token = tokBlockEnd;
        }
        // else: same indent level, so pass through to token analysis
    }

    newLine = false;

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
        // TODO: floating point
        bool e, o;
        strValue = input->token(identRest);
        str s = strValue;
        bool isHex = s.size() > 2 && s[0] == '0' && s[1] == 'x';
        if (isHex)
            s.erase(0, 2);
        uinteger v = from_string(s.c_str(), &e, &o, isHex ? 16 : 10);
        if (e)
            error("'" + strValue + "' is not a valid number");
        if (o || (v > INTEGER_MAX))
            error("Numeric overflow (" + strValue + ")");
        intValue = v;
        return token = tokIntValue;
    }
    
    else  // special chars
    {
        strValue = input->get();
        switch (c)
        {
        case '\\':
            input->skip(wsChars);
            if (!input->eol())
                error("New line expected after '\\'");
            skipEol();
            goto restart;
        case ',': return token = tokComma;
        case '.': return token = (input->get_if('.') ? tokRange : tokPeriod);
        case '\'': parseStringLiteral(); return token = tokStrValue;
        case ';': strValue = "<SEP>"; return token = tokSep;
        case ':':
            if (curlyLevel > 0)
                error("Colon is not allowed in curly-bracket mode, use { }");
            input->skip(wsChars);
            singleLineBlock = !input->eol();
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
        case '{': curlyLevel++; return token = tokBlockBegin;
        case '}':
            if (--curlyLevel < 0)
                error("Unbalanced }");
            return token = tokBlockEnd;
        case '<': return token = (input->get_if('=') ? tokLessEq : tokLAngle);
        case '>': return token = (input->get_if('=') ? tokGreaterEq : tokRAngle);
        case '=': return token = (input->get_if('=') ? tokEqual : tokAssign);
        case '!': return token = (input->get_if('=') ? tokNotEq : tokExclam);
        case '|': return token = tokCat; break;
        }
    }

    error("Illegal character '" + mkPrintable(c) + "'");

    return tokUndefined;
}


str Parser::getIdent()
{
    if (token != tokIdent)
        errorWithLoc("Identifier expected");
    str result = strValue;
    next();
    return result;
}


str Parser::errorLocation() const
{
    str msg;
    if (!strValue.empty())
    {
        str s = strValue;
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
        errorWithLoc("'" + str(errName) + "' expected");
    next();
}


void Parser::skipBlockBegin()
    { skip(tokBlockBegin, "<BEGIN>"); }


void Parser::skipBlockEnd()
    { skip(tokBlockEnd, "<END>"); }


str extractFileName(str filepath)
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
    return filepath.substr(b - p, e - b);
}

