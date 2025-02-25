# Num pages = 37
TEST_SIZES = 10 18 20 30
OUTFILE = /tmp/eval.out
COMMAND = ./xz_s cld.tar.xz 1024 19cf30ae51eddcbefda78dd06014b4b96281456e078ca7c13e1c0c9e6aaea8dff3efb4ad6b0456697718cede6bd5454852652806a657bb56e07d61128434b474 1548636 2376205 0

all:
	@make -C ../../.. > /dev/null
	@cp ../../../libuffd.so .

	@echo "+------------------+" >> $(OUTFILE)
	@echo "| xz_s -> 37 pages |" >> $(OUTFILE)
	@echo "+------------------+" >> $(OUTFILE)

# Baseline run
	@echo -n '(1)                Running baseline: ' >> $(OUTFILE)
	@-/usr/bin/time -a -o $(OUTFILE) -f "%es" $(COMMAND)

# Evaluation run(s)
	@-j=1; for i in $(TEST_SIZES); do test $$((j=$$j+1)); echo -n "($$j)  Running with monitor size = $$i: " >> $(OUTFILE); /usr/bin/time -a -o $(OUTFILE) -f "%es" sudo UFFD_MONITOR_SIZE=$$i LD_PRELOAD=./libuffd.so $(COMMAND) > /dev/null; done

	@rm -f libuffd.so