.PHONY: help smoke stage1-build stage1-bf stage1-heuristics stage1-smoke stage2-build stage2 stage2-batch stage2-smoke stage3-build stage3-generated stage3-mixed stage3-smoke stage4-build stage4-run stage4-smoke clean

help:
	@echo "TSP Algorithms Benchmark"
	@echo ""
	@echo "C++ benchmark stages:"
	@echo "  make stage1-build      Build exact / heuristic baseline program"
	@echo "  make stage1-bf          Run brute-force benchmark for generated instances"
	@echo "  make stage1-heuristics  Run RAND / NN / RNN on TSPLIB examples"
	@echo "  make stage2-build       Build Branch and Bound program"
	@echo "  make stage2             Run one Branch and Bound example"
	@echo "  make stage2-batch       Run Branch and Bound batch comparison"
	@echo "  make stage3-build       Build Simulated Annealing program"
	@echo "  make stage3-generated   Run SA on generated instances"
	@echo "  make stage3-mixed       Run SA on generated + TSPLIB instances"
	@echo "  make stage4-build       Build Genetic Algorithm program"
	@echo "  make stage4-run         Run GA experiment from config/experiment.cfg"
	@echo "  make smoke              Build and run quick checks for all 4 stages"

smoke: stage1-smoke stage2-smoke stage3-smoke stage4-smoke

stage1-build:
	$(MAKE) -C stage1_exact_heuristics

stage1-bf:
	$(MAKE) -C stage1_exact_heuristics run-bruteforce

stage1-heuristics:
	$(MAKE) -C stage1_exact_heuristics run-heuristics

stage1-smoke:
	$(MAKE) -C stage1_exact_heuristics smoke

stage2-build:
	$(MAKE) -C stage2_branch_and_bound

stage2:
	$(MAKE) -C stage2_branch_and_bound run

stage2-batch:
	$(MAKE) -C stage2_branch_and_bound batch

stage2-smoke:
	$(MAKE) -C stage2_branch_and_bound smoke

stage3-build:
	$(MAKE) -C stage3_simulated_annealing

stage3-generated:
	$(MAKE) -C stage3_simulated_annealing run-generated

stage3-mixed:
	$(MAKE) -C stage3_simulated_annealing run-mixed

stage3-smoke:
	$(MAKE) -C stage3_simulated_annealing run-smoke

stage4-build:
	$(MAKE) -C stage4_genetic_algorithm

stage4-run:
	$(MAKE) -C stage4_genetic_algorithm run

stage4-smoke:
	$(MAKE) -C stage4_genetic_algorithm run-smoke

clean:
	$(MAKE) -C stage1_exact_heuristics clean
	$(MAKE) -C stage2_branch_and_bound clean
	$(MAKE) -C stage3_simulated_annealing clean
	$(MAKE) -C stage4_genetic_algorithm clean
