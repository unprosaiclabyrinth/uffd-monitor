# Num pages = 5
TEST_SIZES = 2 3 4
OUTFILE = /tmp/eval.out
COMMAND = ./mcf_s inp.in

all:
	@cp ../../../libuffd.so .

	@echo "+------------------+" >> $(OUTFILE)
	@echo "| mcf_s -> 5 pages |" >> $(OUTFILE)
	@echo "+------------------+" >> $(OUTFILE)

# Baseline run
	@echo -n '(1)               Running baseline: ' >> $(OUTFILE)
	@/usr/bin/time -a -o $(OUTFILE) -f "%es" $(COMMAND) > /dev/null

# Evaluation run(s)
	@j=1; for i in $(TEST_SIZES); do test $$((j=$$j+1)); echo -n "($$j)  Running with monitor size = $$i: " >> $(OUTFILE); /usr/bin/time -a -o $(OUTFILE) -f "%es" sudo UFFD_MONITOR_SIZE=$$i LD_PRELOAD=./libuffd.so $(COMMAND) > /dev/null; done

	@rm -f libuffd.so