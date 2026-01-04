# compiler stuff
CC = gcc
CFLAGS_TEST  = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter
CFLAGS_DEBUG = -g -fno-omit-frame-pointer #-fsanitize=thread/address
CFLAGS_BENCH = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -O3

# directories
DIR_SRC    = src
DIR_BUILD  = build
DIR_DATA   = data
DIR_PLOTS  = plots
DIR_REPORT = report

DIR_ALL = $(DIR_BUILD) $(DIR_DATA) $(DIR_PLOTS)

# files
FILE_ZIP = project.zip
FILE_LOG = nebula.log

# diffrent queue implementations
VARIANTS_SEQ  = seq
VARIANTS_CONC = conc conc2 cas
VARIANTS = $(VARIANTS_SEQ) $(VARIANTS_CONC)

.PHONY: all dirs b_test test test_% b_bench bench_% bench plot clean

all: dirs b_test b_bench

# ensure directories exists
dirs: $(DIR_ALL)

$(DIR_ALL):
	@mkdir -p $@

# build tests
b_test: $(addprefix $(DIR_BUILD)/test_, $(VARIANTS))

$(DIR_BUILD)/test_%: $(DIR_SRC)/test.c $(DIR_SRC)/%.c | $(DIR_BUILD)
	$(CC) $(CFLAGS_TEST) -fopenmp -o $@ $^

# tests
test_%: b_test
	$(DIR_BUILD)/test_$*

test: b_test
	@for v in $(VARIANTS); do \
		echo "Testing '$$v'"; \
		./$(DIR_BUILD)/test_$$v $(if $(N), $(N)); \
		echo ""; \
	done

# build benchmarks
b_bench: $(addprefix $(DIR_BUILD)/bench_, $(VARIANTS))

$(DIR_BUILD)/bench_%: $(DIR_SRC)/bench.c $(DIR_SRC)/%.c | $(DIR_BUILD)
	$(CC) $(CFLAGS_BENCH) -fopenmp -o $@ $^

# benchmarks
small-bench: zip
	@rm -rf $(DIR_DATA)
	@mkdir -p $(DIR_DATA)
	./run_nebula_seq.sh $(FILE_ZIP) bench_seq 1 1 1000
	./run_nebula_conc.sh $(FILE_ZIP) bench_conc 1 1 1000 "1 32 64" a
	./run_nebula_conc.sh $(FILE_ZIP) bench_conc2 1 1 1000 "1 32 64" a
	./run_nebula_conc.sh $(FILE_ZIP) bench_cas 1 1 1000 "1 32 64" a

bench_%: zip
	@if echo "$(VARIANTS_SEQ)" | grep -qw "$*"; then \
		./run_nebula_seq.sh $(FILE_ZIP) bench_$* 10 "1 5" "1 1000"; \
	elif echo "$(VARIANTS_CONC)" | grep -qw "$*"; then \
		./run_nebula_conc.sh $(FILE_ZIP) bench_$* 10 "1 5" "1 1000" "1 2 8 10 20 32 45 64" "a b c d"; \
	else \
		echo "Unknown variant: $*"; \
	fi

bench: zip
	@rm -rf $(DIR_DATA)
	@mkdir -p $(DIR_DATA)
	@for v in $(VARIANTS); do \
		make bench_$$v; \
	done

# plot benchmarks
plot: plot.py | $(DIR_PLOTS)
	@rm -rf $(DIR_PLOTS)/*
	python3 plot.py

small-plot:
	make plot

# zip
zip:
	@rm -rf $(FILE_ZIP)
	@zip $(FILE_ZIP) Makefile README $(DIR_SRC)/* $(DIR_PLOTS)/* $(DIR_REPORT)/* *.sh *.py

#clean
clean:
	@echo "Cleaning build, data and plots directory"
	@rm -rf $(DIR_BUILD)
	@rm -rf $(DIR_DATA)
	@rm -rf $(DIR_PLOTS)
	@echo "Cleaning zip file and nebula log file"
	@rm -rf $(FILE_ZIP)
	@rm -rf $(FILE_LOG)

