TEST_PROGRAM=test_mqtt_esp
all: $(TEST_PROGRAM)

SOURCE_FILES = \
	$(addprefix ../main/, \
		app_thermostat.c \
		app_mqtt.c \
	) \
	stub.c \
  esp_log.c \
	cJSON.c

TEST_SOURCE_FILES = \
	main.cc \
	test_app_thermostat.cc \
	test_app_mqtt.cc

CPPFLAGS += -fprofile-arcs -ftest-coverage -I. -I../main
CFLAGS += -fprofile-arcs -ftest-coverage -I. -I../main
CXXFLAGS += -std=c++11 -Wall -Werror
LDFLAGS += -lstdc++ -Wall -fprofile-arcs -ftest-coverage

OBJ_FILES = $(SOURCE_FILES:.c=.o)

TEST_OBJ_FILES = $(TEST_SOURCE_FILES:.cc=.o)

COVERAGE_FILES = $(OBJ_FILES:.o=.gc*)

TEST_COVERAGE_FILES = $(TEST_OBJ_FILES:.o=.gc*)

$(OBJ_FILES): %.o: %.c

$(TEST_OBJ_FILES): %.o: %.cc

$(TEST_PROGRAM): $(OBJ_FILES) $(TEST_OBJ_FILES)
	g++ $(LDFLAGS) -o $(TEST_PROGRAM) $(OBJ_FILES) $(TEST_OBJ_FILES)

test: $(TEST_PROGRAM)
	./$(TEST_PROGRAM)

long-test: $(TEST_PROGRAM)
	./$(TEST_PROGRAM) [list],[enumtable],[spi_flash_emu],[nvs],[long]

$(COVERAGE_FILES): $(TEST_PROGRAM) long-test

coverage.info: $(COVERAGE_FILES)
	find ../main/ -name "*.gcno" -exec gcov -r -pb {} +
	lcov --capture --directory ../main --no-external --output-file coverage.info

coverage_report: coverage.info
	genhtml coverage.info --output-directory coverage_report
	@echo "Coverage report is in coverage_report/index.html"

clean:
	rm -f $(OBJ_FILES) $(TEST_OBJ_FILES) $(TEST_PROGRAM)
	rm -f $(COVERAGE_FILES) $(TEST_COVERAGE_FILES) *.gcov
	rm -rf coverage_report/
	rm -f coverage.info

.PHONY: clean all test
