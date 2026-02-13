VERBOSITY=INFO

EMLEARN_ANOMALY_PATH = $(shell realpath ../../)
PLATFORM_REPL_PATH = $(shell realpath stm32f746g_disco.repl)

data.csv:
	curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv

model.gz: data.csv
	head -n1 data.csv > data_test.csv
	tail -n50000 data.csv >> data_test.csv
	head -n-50000 data.csv > data_train.csv

	python train.py

${EMLEARN_ANOMALY_PATH}/samples/anomaly/build/zephyr/zephyr.elf: model.gz
	cp model.gz ${EMLEARN_ANOMALY_PATH}/samples/anomaly/emlearn_model.gz
	cd ${EMLEARN_ANOMALY_PATH}; \
		west build -p -b stm32f746g_disco samples/anomaly -- \
			-DEXTRA_CONF_FILE=emlearn.conf \
    		-DCONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH=\"emlearn_model.gz\"

.PHONY: clean_zephyr clean_model clean eval
clean_zephyr:
	-rm -rf ${EMLEARN_ANOMALY_PATH}/build
clean_model:
	-rm -rf build
	-rm -rf saved_model
	-rm -rf logdir
	-rm -rf model.gz
	-rm -f data_test.csv data_train.csv
clean_results:
	-rm -f results.json
	-rm -rf report
clean: clean_zephyr clean_model clean_results
eval: ${EMLEARN_ANOMALY_PATH}/samples/anomaly/build/zephyr/zephyr.elf data.csv
	cd $(EMLEARN_ANOMALY_PATH) && python ./scripts/gen_repl.py --repl $(EMLEARN_ANOMALY_PATH)/build/stm32f746g_disco.repl && cd -
	cd $(EMLEARN_ANOMALY_PATH) && . scripts/activate_renode.sh && cd - && kenning test report \
		--report-path report/report.md \
		--to-html \
		--verbosity $(VERBOSITY) \
		--cfg scenario.yml \
		--measurements results.json
