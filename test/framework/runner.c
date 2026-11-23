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

/** @file @brief Suite selection and side-effect-free compiled case discovery. */
#include "test.h"
#include <stdio.h>
#include <string.h>

static bool case_matches(const struct bt_test_suite *suite,
			 const char *actual, const char *requested)
{
	size_t prefix = strlen(suite->name);

	if (!strcmp(actual, requested)) {
		return true;
	}
	return !strncmp(actual, suite->name, prefix) &&
	       !strncmp(actual + prefix, "__", 2) &&
	       !strcmp(actual + prefix + 2, requested);
}

static bool valid_suite(const struct bt_test_suite *suite)
{
	if (!suite || !suite->name || !suite->name[0] ||
	    (suite->case_count && !suite->cases)) {
		return false;
	}
	for (size_t i = 0; i < suite->case_count; i++) {
		if (!suite->cases[i].name || !suite->cases[i].test_func) {
			return false;
		}
		for (size_t j = 0; j < i; j++) {
			if (!strcmp(suite->cases[j].name, suite->cases[i].name)) {
				return false;
			}
		}
	}
	return true;
}

int bt_test_run_suite(const struct bt_test_suite *suite, const char *case_name)
{
	const char *actual = NULL;

	if (!valid_suite(suite)) {
		fprintf(stderr, "Invalid test suite metadata\n");
		return 2;
	}
	if (case_name) {
		for (size_t i = 0; i < suite->case_count; i++) {
			if (case_matches(suite, suite->cases[i].name, case_name)) {
				actual = suite->cases[i].name;
				break;
			}
		}
		if (!actual) {
			fprintf(stderr, "Unknown or inactive test case: %s\n", case_name);
			return 2;
		}
	}
	if (!suite->case_count) {
		fprintf(stderr, "No active cases in suite %s\n", suite->name);
		return 2;
	}
	bt_test_runtime_initialize();
	cmocka_set_test_filter(actual);
	int result = _cmocka_run_group_tests(suite->name, suite->cases,
					   suite->case_count, suite->setup,
					   suite->teardown);

	/* A raw failure count of 256 must not become a successful process exit. */
	return result == 0 ? 0 : 1;
}

static void json_string(const char *text)
{
	putchar('"');
	for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
		if (*p == '"' || *p == '\\') {
			putchar('\\');
			putchar(*p);
		} else if (*p < 0x20) {
			printf("\\u%04x", *p);
		} else {
			putchar(*p);
		}
	}
	putchar('"');
}

int bt_test_main(int argc, char **argv,
		 const struct bt_test_suite *const *registry, size_t count)
{
	const char *suite_name = NULL;
	const char *case_name = NULL;
	bool list = false;
	bool json = false;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--list") && !list && !json) {
			list = true;
		} else if (!strcmp(argv[i], "--list-json") && !list && !json) {
			json = true;
		} else if (!strcmp(argv[i], "--suite") && !suite_name && i + 1 < argc) {
			suite_name = argv[++i];
		} else if (!strcmp(argv[i], "--case") && !case_name && i + 1 < argc) {
			case_name = argv[++i];
		} else if (argc == 2 && argv[i][0] != '-') {
			case_name = argv[i];
		} else {
			fprintf(stderr, "Usage: %s [--list | --list-json] [--suite NAME] "
				"[--case NAME]\n", argv[0]);
			return 2;
		}
	}
	if ((list || json) && case_name) {
		fprintf(stderr, "Case selection cannot be combined with listing\n");
		return 2;
	}
	const struct bt_test_suite *selected = NULL;
	size_t matches = 0;

	for (size_t i = 0; i < count; i++) {
		if (!valid_suite(registry[i])) {
			fprintf(stderr, "Invalid test suite metadata\n");
			return 2;
		}
		for (size_t j = 0; j < i; j++) {
			if (!strcmp(registry[j]->name, registry[i]->name)) {
				fprintf(stderr, "Duplicate test suite: %s\n", registry[i]->name);
				return 2;
			}
		}
		if (suite_name && strcmp(suite_name, registry[i]->name)) {
			continue;
		}
		if (case_name) {
			for (size_t j = 0; j < registry[i]->case_count; j++) {
				if (case_matches(registry[i], registry[i]->cases[j].name, case_name)) {
					selected = registry[i];
					matches++;
				}
			}
		} else {
			selected = registry[i];
			matches++;
		}
	}
	if (!matches || (case_name && matches != 1)) {
		fprintf(stderr, "Unknown, inactive or ambiguous suite/case selection\n");
		return 2;
	}
	if (case_name) {
		return bt_test_run_suite(selected, case_name);
	}
	if (json) {
		fputs("{\"suites\":[", stdout);
	}
	bool first = true;

	for (size_t i = 0; i < count; i++) {
		const struct bt_test_suite *suite = registry[i];

		if (suite_name && strcmp(suite_name, suite->name)) {
			continue;
		}
		if (list || json) {
			if (json) {
				if (!first) { putchar(','); }
				fputs("{\"name\":", stdout);
				json_string(suite->name);
				fputs(",\"cases\":[", stdout);
			}
			for (size_t j = 0; j < suite->case_count; j++) {
				if (json) {
					if (j) { putchar(','); }
					json_string(suite->cases[j].name);
				} else {
					puts(suite->cases[j].name);
				}
			}
			if (json) { fputs("]}", stdout); }
			first = false;
		} else {
			int result = bt_test_run_suite(suite, NULL);

			if (result) { return result; }
		}
	}
	if (json) { puts("]}"); }
	return 0;
}
