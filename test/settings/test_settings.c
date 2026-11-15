/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @brief Real-file persistence and handler integration checks.
 */

#define _XOPEN_SOURCE 700
#include <embt/base/assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/settings/settings.h>
#include <embt/settings/settings_file.h>
#include <embt/bluetooth/addr.h>
#include <host/hci_core.h>
#include <host/settings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ftw.h>

static char executable[PATH_MAX];
static char test_directory[PATH_MAX];
static char original_directory[PATH_MAX];
static unsigned int value;
static unsigned int deep_value;
static int commit_order[8];
static size_t committed;
static int set_calls;
static int write_mode;
static int write_calls;
static bool rename_failure;
static bool sync_failure;
static bool read_failure;

ssize_t __real_write(int fd, const void *data, size_t size);
int __real_rename(const char *old_path, const char *new_path);
int __real_fsync(int fd);
ssize_t __real_read(int fd, void *buffer, size_t size);

ssize_t __wrap_write(int fd, const void *data, size_t size)
{
	struct stat info;

	if (write_mode && fstat(fd, &info) == 0 && S_ISREG(info.st_mode)) {
		write_calls++;
		if (write_mode == 1) {
			if (write_calls == 1) {
				errno = EINTR;
				return -1;
			}
			return __real_write(fd, data, MIN(size, 1U));
		}
		if (write_mode == 2 && write_calls >= 2) {
			errno = ENOSPC;
			return -1;
		}
	}
	return __real_write(fd, data, size);
}

ssize_t __wrap_read(int fd, void *buffer, size_t size)
{
	struct stat info;

	if (read_failure && size == sizeof(unsigned int) && fstat(fd, &info) == 0 &&
	    S_ISREG(info.st_mode)) {
		read_failure = false;
		errno = EIO;
		return -1;
	}
	return __real_read(fd, buffer, size);
}

int __wrap_rename(const char *old_path, const char *new_path)
{
	if (rename_failure) {
		errno = EIO;
		return -1;
	}
	return __real_rename(old_path, new_path);
}

int __wrap_fsync(int fd)
{
	if (sync_failure) {
		sync_failure = false;
		errno = EIO;
		return -1;
	}
	return __real_fsync(fd);
}

static int set_value(const char *key, size_t len, settings_read_cb read_cb, void *arg)
{
	if (!key || strcmp(key, "value") || len != sizeof(value)) {
		return -EINVAL;
	}
	set_calls++;
	return read_cb(arg, &value, sizeof(value)) == sizeof(value) ? 0 : -EIO;
}

static int get_value(const char *key, char *buffer, int size)
{
	if (!key || strcmp(key, "value") || (size_t)size < sizeof(value)) {
		return -EINVAL;
	}
	memcpy(buffer, &value, sizeof(value));
	return sizeof(value);
}

static int commit_value(void)
{
	commit_order[committed++] = 10;
	return 0;
}

static int export_value(int (*save)(const char *, const void *, size_t))
{
	return save("app/value", &value, sizeof(value));
}

static int set_deep(const char *key, size_t len, settings_read_cb read_cb, void *arg)
{
	if (!key || strcmp(key, "number") || len != sizeof(deep_value)) {
		return -EINVAL;
	}
	return read_cb(arg, &deep_value, sizeof(deep_value)) == sizeof(deep_value) ? 0 : -EIO;
}

static int commit_deep(void)
{
	commit_order[committed++] = 0;
	return 0;
}

static struct settings_handler dynamic_handler = {
	.name = "app",
	.h_get = get_value,
	.h_set = set_value,
	.h_commit = commit_value,
	.h_export = export_value,
};
SETTINGS_STATIC_HANDLER_DEFINE_WITH_CPRIO(deep, "app/deep", NULL, set_deep, commit_deep, NULL, 0);

static int create_directory(void **state)
{
	(void)state;
	assert_non_null(getcwd(original_directory, sizeof(original_directory)));
	strcpy(test_directory, "/tmp/embt-settings-test-XXXXXX");
	assert_non_null(mkdtemp(test_directory));
	assert_int_equal(chdir(test_directory), 0);
	return 0;
}

static int setup(void **state)
{
	create_directory(state);
	assert_int_equal(settings_subsys_init(), 0);
	return 0;
}

static int remove_entry(const char *path, const struct stat *info, int type, struct FTW *ctx)
{
	(void)info;
	(void)ctx;
	return type == FTW_DP ? rmdir(path) : unlink(path);
}

static int teardown(void **state)
{
	(void)state;
	assert_int_equal(chdir(original_directory), 0);
	assert_int_equal(nftw(test_directory, remove_entry, 32, FTW_DEPTH | FTW_PHYS), 0);
	return 0;
}

static off_t file_size(void)
{
	struct stat info;

	assert_int_equal(stat(CONFIG_SETTINGS_FILE_PATH, &info), 0);
	return info.st_size;
}

static void test_crud(void **state)
{
	unsigned int first = 11, second = 22, loaded = 0;
	off_t size;

	(void)state;
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), 0);
	assert_int_equal(settings_save_one("app/value", &first, sizeof(first)), 0);
	size = file_size();
	assert_int_equal(settings_save_one("app/value", &first, sizeof(first)), 0);
	assert_int_equal(file_size(), size);
	assert_int_equal(settings_save_one("app/value", &second, sizeof(second)), 0);
	assert_int_equal(settings_get_val_len("app/value"), sizeof(second));
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, second);
	assert_int_equal(settings_delete("app/value"), 0);
	assert_int_equal(settings_get_val_len("app/value"), 0);
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), 0);
	size = file_size();
	assert_int_equal(settings_delete("app/value"), 0);
	assert_int_equal(file_size(), size);
}

static void test_handlers(void **state)
{
	unsigned int next = 123, deep = 456;

	(void)state;
	assert_int_equal(settings_register_with_cprio(&dynamic_handler, 10), 0);
	assert_int_equal(settings_register(&dynamic_handler), -EEXIST);
	assert_int_equal(settings_register_static(&settings_handler_deep), 0);
	assert_int_equal(settings_register_static(&settings_handler_deep), 0);
	assert_int_equal(settings_runtime_set("app/value", &next, sizeof(next)), 0);
	assert_int_equal(settings_save_subtree("app"), 0);
	assert_int_equal(settings_save_one("app/deep/number", &deep, sizeof(deep)), 0);
	value = deep_value = 0;
	assert_int_equal(settings_load_subtree("app"), 0);
	assert_int_equal(value, next);
	assert_int_equal(deep_value, deep);
	assert_int_equal(set_calls, 2);
	assert_int_equal(committed, 2);
	assert_int_equal(commit_order[0], 0);
	assert_int_equal(commit_order[1], 10);
	unsigned int readback = 0;

	assert_int_equal(settings_runtime_get("app/value", &readback, sizeof(readback)),
			 sizeof(readback));
	assert_int_equal(readback, next);
	assert_int_equal(settings_runtime_set("app/value", NULL, sizeof(next)), -EINVAL);
	assert_int_equal(settings_runtime_get("app/value", NULL, sizeof(next)), -EINVAL);
	assert_int_equal(settings_runtime_get("app/value", &readback, (size_t)INT_MAX + 1),
			 -EOVERFLOW);
}

static int direct_stop(const char *key, size_t length, settings_read_cb read_cb, void *arg,
		       void *param)
{
	int *count = param;
	unsigned int readback;

	assert_string_equal(key, "value");
	assert_int_equal(length, sizeof(readback));
	assert_int_equal(read_cb(arg, &readback, sizeof(readback)), sizeof(readback));
	(*count)++;
	return 1;
}

static void test_direct(void **state)
{
	unsigned int number = 7;
	int count = 0;

	(void)state;
	settings_register_with_cprio(&dynamic_handler, 10);
	settings_save_one("app/value", &number, sizeof(number));
	settings_save_one("app/second", &number, sizeof(number));
	assert_int_equal(settings_load_subtree_direct("app", direct_stop, &count), 0);
	assert_int_equal(count, 1);
	assert_int_equal(committed, 0);
}

static int bounded_read(const char *key, size_t len, settings_read_cb read_cb, void *arg,
			void *param)
{
	unsigned int result = 0;

	(void)key;
	(void)param;
	assert_int_equal(len, sizeof(result));
	assert_int_equal(read_cb(arg, &result, SIZE_MAX), sizeof(result));
	assert_int_equal(result, 42);
	return 1;
}

static void test_read_bounds(void **state)
{
	unsigned int value = 42;

	(void)state;
	assert_int_equal(settings_save_one("bounds/value", &value, sizeof(value)), 0);
	assert_int_equal(settings_save_one("bounds/next", &value, sizeof(value)), 0);
	assert_int_equal(settings_load_subtree_direct("bounds", bounded_read, NULL), 0);
}

static int reentrant_save(const char *key, size_t len, settings_read_cb cb, void *arg, void *param)
{
	unsigned int number = 5;
	int *result = param;

	(void)key;
	(void)len;
	(void)cb;
	(void)arg;
	*result = settings_save_one("nested/value", &number, sizeof(number));
	return 1;
}

static void test_callback_reentry(void **state)
{
	unsigned int number = 4;
	int result = 0;

	(void)state;
	assert_int_equal(settings_save_one("app/value", &number, sizeof(number)), 0);
	assert_int_equal(settings_load_subtree_direct("app", reentrant_save, &result), 0);
	assert_int_equal(result, -EDEADLK);
	assert_int_equal(settings_get_val_len("nested/value"), 0);
}

static void test_compression(void **state)
{
	unsigned int loaded;

	(void)state;
	for (unsigned int i = 1; i <= 80; i++) {
		assert_int_equal(settings_save_one("app/value", &i, sizeof(i)), 0);
	}
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, 80);
	assert_true((size_t)file_size() < 80 * (2 + strlen("app/value") + 1 + sizeof(loaded)));
}

static void test_binary_and_limits(void **state)
{
	unsigned char bytes[SETTINGS_MAX_VAL_LEN], loaded[SETTINGS_MAX_VAL_LEN];
	char key[SETTINGS_FULL_NAME_LEN];

	(void)state;
	for (size_t i = 0; i < sizeof(bytes); i++) {
		bytes[i] = (unsigned char)i;
	}
	memset(key, 'k', sizeof(key) - 1);
	key[sizeof(key) - 1] = '\0';
	assert_int_equal(settings_save_one(key, bytes, sizeof(bytes)), 0);
	for (unsigned int i = 0; i < 40; i++) {
		assert_int_equal(settings_save_one("app/other", &i, sizeof(i)), 0);
	}
	assert_int_equal(settings_load_one(key, loaded, sizeof(loaded)), sizeof(loaded));
	assert_memory_equal(bytes, loaded, sizeof(bytes));
	unsigned char small[16];

	assert_int_equal(settings_load_one(key, small, sizeof(small)), sizeof(bytes));
	assert_memory_equal(bytes, small, sizeof(small));
	assert_int_equal(settings_save_one("bad=key", bytes, 1), -EINVAL);
	assert_int_equal(settings_save_one("", bytes, 1), -EINVAL);
	assert_int_equal(settings_save_one("large", bytes, SETTINGS_MAX_VAL_LEN + 1), -E2BIG);
	assert_int_equal(settings_save_one("null", NULL, 1), -EINVAL);
}

static void append_bytes(const void *data, size_t size)
{
	int fd = open(CONFIG_SETTINGS_FILE_PATH, O_WRONLY | O_APPEND);

	assert_true(fd >= 0);
	assert_int_equal(write(fd, data, size), size);
	assert_int_equal(close(fd), 0);
}

static void test_partial_tail(void **state)
{
	unsigned int number = 99, other = 42, loaded;
	uint16_t unfinished_length = 30;

	(void)state;
	settings_save_one("app/value", &number, sizeof(number));
	append_bytes(&unfinished_length, sizeof(unfinished_length));
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, number);
	assert_int_equal(settings_save_one("app/other", &other, sizeof(other)), 0);
	assert_int_equal(settings_load_one("app/other", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, other);
}

static void test_corruption(void **state)
{
	unsigned int number = 42;
	uint16_t bad_length = 1;
	off_t size;

	(void)state;
	settings_register_with_cprio(&dynamic_handler, 10);
	settings_save_one("app/value", &number, sizeof(number));
	append_bytes(&bad_length, sizeof(bad_length));
	size = file_size();
	assert_int_equal(settings_load(), -EBADMSG);
	assert_int_equal(set_calls, 0);
	assert_int_equal(committed, 0);
	assert_int_equal(settings_save_one("app/other", &number, sizeof(number)), -EBADMSG);
	assert_int_equal(file_size(), size);
}

static void test_read_failure(void **state)
{
	unsigned int number = 33;

	(void)state;
	assert_int_equal(settings_register_with_cprio(&dynamic_handler, 10), 0);
	assert_int_equal(settings_save_one("app/value", &number, sizeof(number)), 0);
	read_failure = true;
	assert_int_equal(settings_load_subtree("app"), -EIO);
	assert_int_equal(value, 0);
	assert_int_equal(committed, 0);
	assert_int_equal(settings_load_subtree("app"), 0);
	assert_int_equal(value, number);
	assert_int_equal(committed, 1);
}

static void test_write_failures(void **state)
{
	unsigned int first = 11, second = 22, loaded;
	off_t size;

	(void)state;
	settings_save_one("app/value", &first, sizeof(first));
	write_mode = 1;
	write_calls = 0;
	assert_int_equal(settings_save_one("app/value", &second, sizeof(second)), 0);
	write_mode = 0;
	size = file_size();
	write_mode = 2;
	write_calls = 0;
	assert_true(settings_save_one("app/value", &first, sizeof(first)) < 0);
	write_mode = 0;
	assert_int_equal(file_size(), size);
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, second);
}

static void test_rename_failure(void **state)
{
	unsigned int loaded;

	(void)state;
	for (unsigned int i = 1; i < CONFIG_SETTINGS_FILE_MAX_LINES; i++) {
		assert_int_equal(settings_save_one("app/value", &i, sizeof(i)), 0);
	}
	off_t size = file_size();
	unsigned int next = 123;

	rename_failure = true;
	assert_true(settings_save_one("app/value", &next, sizeof(next)) < 0);
	rename_failure = false;
	assert_int_equal(file_size(), size);
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, CONFIG_SETTINGS_FILE_MAX_LINES - 1);
	assert_int_equal(settings_save_one("app/value", &next, sizeof(next)), 0);
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, next);
}

static void test_sync_failure(void **state)
{
	unsigned int number = 81, loaded;

	(void)state;
	sync_failure = true;
	assert_true(settings_save_one("app/value", &number, sizeof(number)) < 0);
	assert_int_equal(settings_save_one("app/value", &number, sizeof(number)), 0);
	assert_int_equal(settings_load_one("app/value", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_int_equal(loaded, number);
}

static void test_modes_and_symlink(void **state)
{
	struct stat info;
	unsigned int number = 1;

	(void)state;
	settings_save_one("app/value", &number, sizeof(number));
	assert_int_equal(stat(CONFIG_SETTINGS_FILE_PATH, &info), 0);
	assert_int_equal(info.st_mode & 077, 0);
	assert_int_equal(stat(".embt", &info), 0);
	assert_int_equal(info.st_mode & 077, 0);
	assert_int_equal(unlink(CONFIG_SETTINGS_FILE_PATH), 0);
	int fd = open(".embt/target", O_CREAT | O_RDWR, 0600);

	assert_true(fd >= 0);
	assert_int_equal(close(fd), 0);
	assert_int_equal(symlink("target", CONFIG_SETTINGS_FILE_PATH), 0);
	assert_true(settings_save_one("app/value", &number, sizeof(number)) < 0);
	assert_int_equal(stat(".embt/target", &info), 0);
	assert_int_equal(info.st_size, 0);
}

static void test_init_retry(void **state)
{
	(void)state;
	int fd = open("blocked", O_CREAT | O_RDWR, 0600);

	assert_true(fd >= 0);
	close(fd);
	assert_int_equal(settings_file_set_path("blocked/settings.bin"), 0);
	assert_true(settings_subsys_init() < 0);
	assert_int_equal(settings_register_static(&settings_handler_deep), 0);
	unlink("blocked");
	assert_int_equal(mkdir("blocked", 0700), 0);
	assert_int_equal(settings_subsys_init(), 0);
	assert_int_equal(settings_subsys_init(), 0);
	assert_non_null(settings_parse_and_lookup("app/deep/number", NULL));
	assert_int_equal(settings_file_set_path("new-path"), -EBUSY);
}

static int child_mode(const char *mode, const char *prefix)
{
	unsigned int loaded;
	int err = settings_subsys_init();

	if (err) {
		return 2;
	}
	if (!strcmp(mode, "child_write")) {
		unsigned int first = 90, final = 123;

		if (settings_save_one("persist/value", &first, sizeof(first)) ||
		    settings_save_one("persist/value", &final, sizeof(final)) ||
		    settings_save_one("persist/deleted", &first, sizeof(first)) ||
		    settings_delete("persist/deleted")) {
			return 3;
		}
		puts("Persisted value=123 and a deletion record.");
		return 0;
	}
	if (!strcmp(mode, "child_read")) {
		if (settings_load_one("persist/value", &loaded, sizeof(loaded)) != sizeof(loaded) ||
		    loaded != 123 || settings_get_val_len("persist/deleted") != 0) {
			return 4;
		}
		puts("Loaded value=123 in a new process; deleted key is absent.");
		return 0;
	}
	for (unsigned int i = 0; i < 20; i++) {
		char key[64];

		snprintf(key, sizeof(key), "%s/value%u", prefix, i);
		if (settings_save_one(key, &i, sizeof(i))) {
			return 5;
		}
	}
	return 0;
}

static pid_t start_child(const char *mode, const char *prefix)
{
	pid_t child = fork();

	assert_true(child >= 0);
	if (child == 0) {
		execl(executable, executable, mode, prefix, NULL);
		_exit(127);
	}
	return child;
}

static void join_child(pid_t child)
{
	int status;

	assert_int_equal(waitpid(child, &status, 0), child);
	assert_true(WIFEXITED(status));
	assert_int_equal(WEXITSTATUS(status), 0);
}

static void test_restart(void **state)
{
	(void)state;
	join_child(start_child("child_write", ""));
	join_child(start_child("child_read", ""));
}

static void test_process_writers(void **state)
{
	pid_t children[3];
	char prefix[16];

	(void)state;
	for (size_t i = 0; i < ARRAY_SIZE(children); i++) {
		snprintf(prefix, sizeof(prefix), "writer%zu", i);
		children[i] = start_child("child_worker", prefix);
	}
	for (size_t i = 0; i < ARRAY_SIZE(children); i++) {
		join_child(children[i]);
	}
	for (unsigned int w = 0; w < ARRAY_SIZE(children); w++) {
		for (unsigned int i = 0; i < 20; i++) {
			unsigned int loaded;
			char key[64];

			snprintf(key, sizeof(key), "writer%u/value%u", w, i);
			assert_int_equal(settings_load_one(key, &loaded, sizeof(loaded)),
					 sizeof(loaded));
			assert_int_equal(loaded, i);
		}
	}
}

static void test_bluetooth_identity(void **state)
{
	bt_addr_le_t identity = {.type = BT_ADDR_LE_RANDOM, .a = {.val = {1, 2, 3, 4, 5, 0xc6}}};

	(void)state;
	assert_int_equal(bt_stack_init(), 0);
	assert_int_equal(bt_settings_init(), 0);
	assert_int_equal(bt_settings_init(), 0);
	bt_dev.id_count = 1;
	bt_dev.id_addr[0] = identity;
	assert_int_equal(bt_settings_store_id(), 0);
	bt_settings_flush();
	bt_dev.id_count = 0;
	memset(bt_dev.id_addr, 0, sizeof(bt_dev.id_addr));
	bt_addr_le_t loaded;

	assert_int_equal(settings_load_one("bt/id", &loaded, sizeof(loaded)), sizeof(loaded));
	assert_memory_equal(&loaded, &identity, sizeof(identity));
	/* Preserve the Host requirement: bt/ handlers defer application until HCI is open. */
	assert_int_equal(settings_load_subtree("bt"), 0);
	assert_int_equal(bt_dev.id_count, 0);
	assert_non_null(settings_parse_and_lookup("bt/id", NULL));
}

struct selected_case {
	const char *name;
	void (*run)(void **);
};
static const struct selected_case cases[] = {
	{"crud", test_crud},
	{"handlers", test_handlers},
	{"direct", test_direct},
	{"callback_reentry", test_callback_reentry},
	{"read_bounds", test_read_bounds},
	{"compression", test_compression},
	{"binary_limits", test_binary_and_limits},
	{"partial_tail", test_partial_tail},
	{"corruption", test_corruption},
	{"write_failures", test_write_failures},
	{"read_failure", test_read_failure},
	{"rename_failure", test_rename_failure},
	{"sync_failure", test_sync_failure},
	{"modes_symlink", test_modes_and_symlink},
	{"init_retry", test_init_retry},
	{"restart", test_restart},
	{"process_writers", test_process_writers},
	{"bluetooth_identity", test_bluetooth_identity},
};

int main(int argc, char **argv)
{
	if (argc < 2 || !realpath(argv[0], executable)) {
		return 2;
	}
	if (!strncmp(argv[1], "child_", 6)) {
		return child_mode(argv[1], argc > 2 ? argv[2] : "writer");
	}
	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		if (!strcmp(argv[1], cases[i].name)) {
			const struct CMUnitTest selected[] = {
				{cases[i].name, cases[i].run,
				 !strcmp(cases[i].name, "init_retry") ? create_directory : setup,
				 teardown, NULL}};

			return cmocka_run_group_tests(selected, NULL, NULL);
		}
	}
	return 2;
}
