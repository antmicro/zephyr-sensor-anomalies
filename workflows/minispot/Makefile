VERBOSITY=INFO

KENNING_ANOMALY_PATH = $(shell realpath ../../)
PLATFORM_REPL_PATH = $(shell realpath stm32f746g_disco.repl)

net.pt:
	kenning train \
		--verbosity $(VERBOSITY) \
		--cfg scenario.yml

build/fp32.1.tflite: net.pt
	kenning optimize \
		--verbosity $(VERBOSITY) \
		--cfg scenario.yml

${KENNING_ANOMALY_PATH}/samples/anomaly/build/zephyr/zephyr.elf: build/fp32.1.tflite
	cp build/fp32.1.tflite ${KENNING_ANOMALY_PATH}/samples/anomaly/kenning_tflite_model.tflite
	cp build/fp32.1.tflite.json ${KENNING_ANOMALY_PATH}/samples/anomaly/kenning_tflite_model.tflite.json
	cd ${KENNING_ANOMALY_PATH}; \
		west build -p -b stm32f746g_disco samples/anomaly -- \
			-DCONFIG_KENNING_TFLITE_BUFFER_SIZE=100 \
			-DCONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=1024 \
			-DEXTRA_CONF_FILE=kenning.conf \
    		-DCONFIG_KENNING_MODEL_PATH=\"kenning_tflite_model.tflite\"

.PHONY: clean_zephyr clean_model clean eval eval_local
clean_zephyr:
	-rm -rf ${KENNING_ANOMALY_PATH}/build
clean_model:
	-rm -rf build
	-rm -f net.pt
	-rm -f net_final.pt
	-rm -rf saved_model
	-rm -rf logdir
clean_results:
	-rm -f results.json
	-rm -rf report
clean: clean_zephyr clean_model clean_results
eval: ${KENNING_ANOMALY_PATH}/samples/anomaly/build/zephyr/zephyr.elf
	cd $(KENNING_ANOMALY_PATH) && python ./scripts/gen_repl.py --repl $(KENNING_ANOMALY_PATH)/build/stm32f746g_disco.repl && cd -
	cd $(KENNING_ANOMALY_PATH) && . scripts/activate_renode.sh && cd - && kenning test report \
		--report-path report/report.md \
		--to-html \
		--verbosity $(VERBOSITY) \
		--cfg scenario.yml \
		--measurements results.json
