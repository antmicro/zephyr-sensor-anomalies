SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

SOURCE_DIR    ?= $(realpath ../)
DOCS_DIR      := $(SOURCE_DIR)/docs

REPORT_SAMPLE_DIR := sample-real-time-evaluation-report
REPORT_FILE   := $(DOCS_DIR)/source/generated/$(REPORT_SAMPLE_DIR)/report.md
DATASET_FILE  := $(SOURCE_DIR)/data.csv
MODEL_FILE    := $(SOURCE_DIR)/workflows/minispot/build/fp32.1.tflite

.PHONY: report c_api_docs dataset training


html latexpdf all: $(REPORT_FILE) c_api_docs

# Catch-all target: route all unknown targets to Sphinx using the "make mode" option.
# $(O) is meant as a shortcut for $(SPHINXOPTS).
%:
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

c_api_docs:
	doxygen Doxyfile

dataset: $(DATASET_FILE)
	@:
$(DATASET_FILE):
	curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv \
		-o $(DATASET_FILE)

training: $(MODEL_FILE)
	@:
$(MODEL_FILE): $(DATASET_FILE)
	cd $(SOURCE_DIR)/workflows/minispot && \
	kenning train --cfg scenario.yml && \
	kenning optimize --cfg scenario.yml

# Avoid building the the report sample if it already exists.
# Use @: to avoid building the full docs when `make report` is invoked.
report: $(REPORT_FILE)
	@:
$(REPORT_FILE): $(MODEL_FILE)
	cd $(SOURCE_DIR) && \
	west build -p -b stm32f746g_disco samples/anomaly \
		-DCONFIG_KENNING_TFLITE_BUFFER_SIZE=100 \
		-DCONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=1024 \
		-DCONFIG_KENNING_PROTOCOL_INTEGRATION=y \
		-DEXTRA_CONF_FILE=kenning.conf \
		-DCONFIG_KENNING_MODEL_PATH=\"$(SOURCE_DIR)/workflows/minispot/build/fp32.1.tflite\" && \
	python3 $(SOURCE_DIR)/scripts/gen_repl.py \
		--repl $(SOURCE_DIR)/build/stm32f746g_disco.repl

	cd $(SOURCE_DIR)/workflows/minispot && \
	kenning test --cfg scenario.yml \
		--measurements results.json && \
	kenning report \
		--report-name "Real-time model evaluation, with time-based metrics" \
		--report-path $(REPORT_FILE) \
		--root-dir $(DOCS_DIR)/source \
		--img-dir $(DOCS_DIR)/source/generated/$(REPORT_SAMPLE_DIR)/img \
		--measurements results.json \
		--smaller-header
