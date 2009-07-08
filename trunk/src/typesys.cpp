

#include "typesys.h"

// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


Base::Base(BaseId id): Symbol(null_str), baseId(id)  { }
Base::Base(const str& name, BaseId id): Symbol(name), baseId(id)  { }


