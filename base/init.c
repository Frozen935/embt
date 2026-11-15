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

/** @file @brief Stable initialization order and reverse cleanup without linker sections. */
#include <embt/base/init.h>
#include <errno.h>

int bt_init_run(const struct bt_init_entry *entries, size_t count,
		size_t *order, size_t *completed)
{
	int err;
	if (!completed || (count && (!entries || !order))) {
		return -EINVAL;
	}
	*completed = 0;
	for (size_t i = 0; i < count; i++) {
		if (!entries[i].initialize || !entries[i].deinitialize) {
			return -EINVAL;
		}
		order[i] = i;
		for (size_t j = i;
		     j && entries[order[j]].priority < entries[order[j - 1]].priority; j--) {
			size_t previous = order[j - 1];
			order[j - 1] = order[j];
			order[j] = previous;
		}
	}
	for (size_t i = 0; i < count; i++) {
		err = entries[order[i]].initialize();
		if (err) {
			return err;
		}
		(*completed)++;
	}
	return 0;
}

int bt_init_rollback(const struct bt_init_entry *entries, const size_t *order, size_t *completed)
{
	int err;
	if (!completed || (*completed && (!entries || !order))) {
		return -EINVAL;
	}
	while (*completed) {
		err = entries[order[*completed - 1]].deinitialize();
		if (err) {
			return err;
		}
		(*completed)--;
	}
	return 0;
}
