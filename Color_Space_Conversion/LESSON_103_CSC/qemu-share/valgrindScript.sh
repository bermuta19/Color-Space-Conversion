for bin in csc_float_demo csc_int_demo csc_vector_demo; do
	valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --cachegrind-out-file=cachegrind.out.$bin ./$bin
	cg_annotate --show=Ir,Dr,Dw,D1mr,D1mw,DLmr,DLmw cachegrind.out.$bin > cachegrind_report_$bin.txt
done
for bin in csc_float_demo csc_int_demo csc_vector_demo; do
	echo "== $bin =="
	for i in 1 2 3; do /usr/bin/time -f "%e s" ./$bin 2>&1 | tail -1; done
done
