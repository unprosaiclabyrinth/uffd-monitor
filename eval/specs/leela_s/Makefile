# Num pages = 51
TEST_SIZES = 18 20 30 40
OUTFILE = /tmp/eval.out
COMMAND = ./leela_s_base.uffd-eval-m64 test.sgf

all:
	@make -C ../../.. > /dev/null
	@cp ../../../libuffd.so .

	@echo "+---------------------+" >> $(OUTFILE)
	@echo "| leela_s -> 51 pages |" >> $(OUTFILE)
	@echo "+---------------------+" >> $(OUTFILE)	

# Baseline run
	@echo -n '(1)                Running baseline: ' >> $(OUTFILE)
	@/usr/bin/time -a -o $(OUTFILE) -f "%es" $(COMMAND) > /dev/null

# Evaluation run(s)
	@j=1; for i in $(TEST_SIZES); do test $$((j=$$j+1)); echo -n "($$j)  Running with monitor size = $$i: " >> $(OUTFILE); /usr/bin/time -a -o $(OUTFILE) -f "%es" sudo UFFD_MONITOR_SIZE=$$i LD_PRELOAD=./libuffd.so $(COMMAND) > /dev/null; done

	@rm -f libuffd.so