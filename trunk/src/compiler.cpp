
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
            ShType* indexType = getType(NULL);
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDisplayName("") + " can't be used as array index");
            type = type->deriveArrayType(indexType, currentScope);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getType(ShBase* previewObj)
{
    // type ::= type-id { type-derivator }
    if (previewObj == NULL)
        previewObj = getQualifiedName();
    ShType* type = NULL;
    if (previewObj->isTypeAlias())
        type = ((ShTypeAlias*)previewObj)->base;
    else if (previewObj->isType())
        type = (ShType*)previewObj;
    else
        parser.errorWithLoc("Expected type identifier");
    return getDerivators(type);
}


ShBase* ShModule::getAtom()
{
    if (parser.token == tokIdent)
        return getQualifiedName();
    error("Error in statement");
    return NULL;
}


void ShModule::parseTypeDef()
{
    // type-alias ::= "def" type ident { type-derivator }
    ShType* type = getType(NULL);
    string ident = parser.getIdent();
    type = getDerivators(type);
    ShTypeAlias* alias = new ShTypeAlias(ident, type);
    try
    {
        currentScope->addTypeAlias(alias);
    }
    catch (Exception& e)
    {
        delete alias;
        throw;
    }
    parser.skipSep();
}


void ShModule::parseObjectDef(ShBase* previewObj)
{
    // object-def ::= type ident [ "=" expr ]
    ShType* type = getType(previewObj);
    string ident = parser.getIdent();
    type = getDerivators(type);
    ShVariable* var = new ShVariable(ident, type);
    try
    {
        currentScope->addVariable(var);
    }
    catch(Exception& e)
    {
        delete var;
        throw;
    }
    // TODO: initializer
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
            setNamePleaseThisIsBadIKnow(modName);
            parser.skipSep();
        }

        // { statement | definition }
        while (parser.token != tokEof)
        {
            if (parser.token == tokDef)
            {
                parser.next();
                parseTypeDef();
            }
            
            else
            {
                ShBase* obj = getAtom();
                if (obj->isType() || obj->isTypeAlias())
                    parseObjectDef(obj);
                else
                    parser.errorWithLoc("Expected definition or statement");
            }
        }

        compiled = true;

#ifdef DEBUG
        dump("");
#endif

    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
}


