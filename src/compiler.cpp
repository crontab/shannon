
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
            error("'" + ident + "' is not known within '" + scope->name + "'");
    }
    return obj;
}


ShType* ShModule::getDerivators(ShType* type)
{
    // array-derivator ::= "[" [ type ] "]"
    if (parser.token == tokLSquare)
    {
        parser.next();
        if (parser.token == tokRSquare)
        {
            type = type->deriveVectorType(currentScope);
            parser.next();
        }
        else
        {
            ShType* indexType = getType();
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDisplayName("") + " can't be used as array index");
            type = type->deriveArrayType(indexType, currentScope);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getType()
{
    // type ::= type-id { type-derivator }
    ShBase* obj = getQualifiedName();
    ShType* type = NULL;
    if (obj->isTypeAlias())
        type = ((ShTypeAlias*)obj)->base;
    else if (obj->isType())
        type = (ShType*)obj;
    else
        error("Expected type identifier" + parser.errorLocation());
    return getDerivators(type);
}


void ShModule::parseDef()
{
    getType();
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
            if (parser.token == tokDef)
            {
                parser.next();
                parseDef();
            }
            else
                error("Expected definition or statement" + parser.errorLocation());
        }
    
        compiled = true;
        
        dump("");
    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
}


