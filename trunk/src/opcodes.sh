
grep '^ *op[A-Za-z0-9]*,' vm.h|sed 's/^ *//;s/,.*$//' > OPS
grep '^ *case  *op[A-Za-z0-9]*:' vm.cpp|sed 's/^ *case  *//;s/:.*$//' > OPS.impl
diff OPS OPS.impl
rm OPS.impl
