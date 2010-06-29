
# Make sure declaration of opcdes is in sync with the implementation in vm.cpp;
# display diffferences if any

grep '^ *op[A-Za-z0-9]*,' vm.h|sed 's/^ *//;s/,.*$//' > OPS.decl
grep '^ *case  *op[A-Za-z0-9]*:' vm.cpp|sed 's/^ *case  *//;s/:.*$//' > OPS.impl
diff OPS.decl OPS.impl
rm OPS.impl OPS.decl
