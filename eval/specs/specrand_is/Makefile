# Num pages = 1
TEST_SIZES = 1
OUTFILE = /tmp/eval.out
COMMAND = ./specrand_is 324342 24239

all:
	@cp ../../../libuffd.so .

	@echo "+-----------------------+" >> $(OUTFILE)
	@echo "| specrand_is -> 1 page |" >> $(OUTFILE)
	@echo "+-----------------------+" >> $(OUTFILE)

# Baseline run
	@echo -n '(1)                Running baseline: ' >> $(OUTFILE)
	@-/usr/bin/time -a -o $(OUTFILE) -f "%es" $(COMMAND)

# Evaluation run(s)
	@-j=1; for i in $(TEST_SIZES); do test $$((j=$$j+1)); echo -n "($$j)  Running with monitor size = $$i: " >> $(OUTFILE); /usr/bin/time -a -o $(OUTFILE) -f "%es" sudo UFFD_MONITOR_SIZE=$$i LD_PRELOAD=./libuffd.so $(COMMAND) > /dev/null; done

	@rm -f libuffd.so