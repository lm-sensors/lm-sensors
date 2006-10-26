LIB_DIR		:= lib
LIB_TEST_DIR	:= lib/test

LIB_TEST_TARGETS := $(LIB_TEST_DIR)/test-scanner
LIB_TEST_SOURCES := $(LIB_TEST_DIR)/test-scanner.c

LIBICONV := $(shell if /sbin/ldconfig -p | grep -q libiconv\\.so ; then echo \-liconv; else echo; fi)

LIB_TEST_SCANNER_OBJS := \
	$(LIB_TEST_DIR)/test-scanner.ro \
	$(LIB_DIR)/conf-lex.ao \
	$(LIB_DIR)/error.ao \
	$(LIB_DIR)/general.ao

$(LIB_TEST_DIR)/test-scanner: $(LIB_TEST_SCANNER_OBJS)
	$(CC) $(EXLDFLAGS) -o $@ $(LIB_TEST_SCANNER_OBJS) $(LIBICONV) -Llib

all-lib-test: $(LIB_TEST_TARGETS)
user :: all-lib-test

$(LIB_TEST_DIR)/test-scanner.ro: $(LIB_DIR)/data.h $(LIB_DIR)/conf.h $(LIB_DIR)/conf-parse.h $(LIB_DIR)/scanner.h

clean-lib-test:
	$(RM) $(LIB_TEST_DIR)/*.rd $(LIB_TEST_DIR)/*.ro 
	$(RM) $(LIB_TEST_TARGETS)
clean :: clean-lib-test
