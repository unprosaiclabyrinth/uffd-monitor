# Num pages = 5
TEST_SIZES = 2
OUTFILE = /tmp/eval.out

all:
	@make -C ../../.. > /dev/null
	@cp ../../../libuffd.so .

	@echo "+------------------+" >> $(OUTFILE)
	@echo "| mcf_s -> 5 pages |" >> $(OUTFILE)
	@echo "+------------------+" >> $(OUTFILE)

# Baseline run
	@echo -n '(1)               Running baseline: ' >> $(OUTFILE)
	@/usr/bin/time -a -o $(OUTFILE) -f "%es" ./mcf_s_base.uffd-eval-m64 inp.in > /dev/null

# Evaluation run(s)
	@j=1; for i in $(TEST_SIZES); do test $$((j=$$j+1)); echo -n "($$j)  Running with monitor size = $$i: " >> $(OUTFILE); /usr/bin/time -a -o $(OUTFILE) -f "%es" sudo UFFD_MONITOR_SIZE=$$i LD_PRELOAD=./libuffd.so ./mcf_s_base.uffd-eval-m64 inp.in > /dev/null; done

	@rm -f libuffd.so