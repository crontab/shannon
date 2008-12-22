
#include <stdio.h>

#include "except.h"
#include "langobj.h"


ShBase* ShModule::getQualifiedName()
{
    // qualified-name ::= { ident "." } ident
    string ident = parser.getIdent();
    ShBase* obj = currentScope->deepSearch(ident);
    while (parser.token == tokPeriod)
    {
        if (!obj->isScope())
            return obj;
        ShScope* scope = (ShScope*)obj;
        parser.next(); // "."
        ident = parser.getIdent();
        obj = scope->find(ident);
        if (obj == NULL)
            throw ENotFound(ident);
    }
    return obj;
}


ShType* ShModule::deriveType(ShType* type)
{
    if (parser.token == tokLSquare)
    {
    }
    return type;
}


void ShModule::parseDefinition()
{
    bool isConst = parser.token == tokConst;
    parser.next(); // "def" | "const"
    
    ShBase* obj = getQualifiedName();
    if (!obj->isType())
        error("Expected type identifier" + parser.errorLocation());
    ShType* type = (ShType*)obj;
    
    if (token == tokAnon)
    {
        if (isConst)
            error("'const' can't be used with type definition");
        parser.next(); // "*"
        type = deriveType();
    }
    
    parser.skipSep();
}


void ShModule::compile()
{
    try
    {
        currentScope = this;
        
        parser.next();
        
        // module-header ::= "module" ident
        if (parser.token == tokModule)
        {
            parser.next();
            string modName = parser.getIdent();
            if (strcasecmp(modName.c_str(), name.c_str()) != 0)
                error("Module name mismatch");
            setNamePlease(modName);
            parser.skipSep();
        }
        
        // { statement | definition }
        while (parser.token != tokEof)
        {
            if (parser.token == tokDef || parser.token == tokConst)
                parseDefinition();
            else
                error("Expected definition or statement" + parser.errorLocation());
        }
    
        compiled = true;
    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
}


