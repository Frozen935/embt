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

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <base/base.h>

#include <embt/settings/settings.h>
#include <embt/settings/settings_file.h>
#include "settings_priv.h"

#define SETTINGS_FILE_MAX_LINES CONFIG_SETTINGS_FILE_MAX_LINES
#define SETTINGS_FILE_PATH      CONFIG_SETTINGS_FILE_PATH

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#define FILE_DIRECTORY 1
#define FILE_REGULAR   2
struct settings_file_stat {
	int type;
};
struct settings_file_io {
	int fd;
	off_t limit;
	const char *path;
	bool dirty;
	bool created;
	int io_error;
};
static _Thread_local bool file_operation_active;
static const char *configured_path = SETTINGS_FILE_PATH;

static int sync_parent(const char *path)
{
	char parent[SETTINGS_FILE_NAME_MAX];
	char *slash;
	int fd, err;

	if (strlen(path) >= sizeof(parent)) {
		return -ENAMETOOLONG;
	}
	strcpy(parent, path);
	slash = strrchr(parent, '/');
	if (!slash) {
		strcpy(parent, ".");
	} else if (slash == parent) {
		slash[1] = '\0';
	} else {
		*slash = '\0';
	}
	fd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		return -errno;
	}
	err = fsync(fd) == 0 ? 0 : -errno;
	close(fd);
	return err;
}

static void file_init(struct settings_file_io *file)
{
	*file = (struct settings_file_io){.fd = -1};
}

/** Validate complete records, leaving a partial final record outside limit. */
static int file_validate(struct settings_file_io *file)
{
	struct stat info;
	off_t offset = 0;

	if (fstat(file->fd, &info) < 0) {
		return -errno;
	}
	if (!S_ISREG(info.st_mode)) {
		return -EINVAL;
	}
	while (offset < info.st_size) {
		uint16_t length;
		char name[SETTINGS_FULL_NAME_LEN];
		ssize_t count;
		size_t name_size;

		if (info.st_size - offset < (off_t)sizeof(length)) {
			break;
		}
		do {
			count = pread(file->fd, &length, sizeof(length), offset);
		} while (count < 0 && errno == EINTR);
		if (count < 0) {
			return -errno;
		}
		if ((size_t)count != sizeof(length)) {
			return -EIO;
		}
		if (length < 2 || length > SETTINGS_FULL_NAME_LEN + SETTINGS_MAX_VAL_LEN) {
			return -EBADMSG;
		}
		if (info.st_size - offset - (off_t)sizeof(length) < length) {
			break;
		}
		name_size = MIN((size_t)length, sizeof(name));
		do {
			count = pread(file->fd, name, name_size, offset + sizeof(length));
		} while (count < 0 && errno == EINTR);
		if (count < 0) {
			return -errno;
		}
		if ((size_t)count != name_size) {
			return -EIO;
		}
		char *separator = memchr(name, '=', name_size);

		if (!separator || separator == name || memchr(name, '\0', separator - name) ||
		    separator - name >= SETTINGS_FULL_NAME_LEN ||
		    length - (separator - name) - 1 > SETTINGS_MAX_VAL_LEN) {
			return -EBADMSG;
		}
		offset += sizeof(length) + length;
	}
	file->limit = offset;
	return 0;
}

static int file_open(struct settings_file_io *file, const char *path, int flags)
{
	struct stat before;
	bool existed = lstat(path, &before) == 0;
	int err;

	file->fd = open(path, flags | O_CLOEXEC | O_NOFOLLOW, 0600);
	if (file->fd < 0) {
		return -errno;
	}
	file->path = path;
	file->created = !existed && (flags & O_CREAT);
	err = file_validate(file);
	if (err) {
		close(file->fd);
		file->fd = -1;
	}
	return err;
}

static int file_close(struct settings_file_io *file)
{
	int err = file->io_error;

	if (file->fd < 0) {
		return 0;
	}
	if (file->dirty && fsync(file->fd) < 0) {
		err = -errno;
	}
	if (close(file->fd) < 0 && !err) {
		err = -errno;
	}
	file->fd = -1;
	if (!err && file->created) {
		err = sync_parent(file->path);
	}
	return err;
}

static int file_seek(struct settings_file_io *file, off_t offset, int whence)
{
	if (lseek(file->fd, offset, whence) < 0) {
		file->io_error = -errno;
		return file->io_error;
	}
	return 0;
}

static ssize_t file_read(struct settings_file_io *file, void *buffer, size_t size)
{
	off_t position = lseek(file->fd, 0, SEEK_CUR);
	ssize_t result;

	if (position < 0) {
		return -errno;
	}
	if (position >= file->limit) {
		return 0;
	}
	size = MIN(size, (size_t)(file->limit - position));
	do {
		result = read(file->fd, buffer, size);
	} while (result < 0 && errno == EINTR);
	if (result < 0) {
		file->io_error = -errno;
		return file->io_error;
	}
	return result;
}

static ssize_t file_write(struct settings_file_io *file, const void *buffer, size_t size)
{
	size_t offset = 0;

	while (offset < size) {
		ssize_t result = write(file->fd, (const char *)buffer + offset, size - offset);

		if (result < 0 && errno == EINTR) {
			continue;
		}
		if (result <= 0) {
			file->io_error = result == 0 ? -EIO : -errno;
			return file->io_error;
		}
		file->dirty = true;
		offset += result;
	}
	return size;
}

static int file_stat(const char *path, struct settings_file_stat *entry)
{
	struct stat info;

	if (lstat(path, &info) < 0) {
		return -errno;
	}
	entry->type =
		S_ISDIR(info.st_mode) ? FILE_DIRECTORY : (S_ISREG(info.st_mode) ? FILE_REGULAR : 0);
	return 0;
}

static int file_unlink(const char *path)
{
	return unlink(path) < 0 ? -errno : 0;
}

static int file_rename(const char *old_path, const char *new_path)
{
	if (rename(old_path, new_path) < 0) {
		return -errno;
	}
	return sync_parent(new_path);
}

static int file_mkdir(const char *path)
{
	if (mkdir(path, 0700) < 0) {
		return -errno;
	}
	return sync_parent(path);
}

/** A separate stable lock inode keeps file operations serialized across rename. */
static int file_acquire(const char *path, bool exclusive)
{
	char lock_path[SETTINGS_FILE_NAME_MAX];
	struct stat info;
	int fd, err;

	if (file_operation_active) {
		return -EDEADLK;
	}
	int length = snprintf(lock_path, sizeof(lock_path), "%s.lock", path);

	if (length < 0 || (size_t)length >= sizeof(lock_path)) {
		return -ENAMETOOLONG;
	}
	fd = open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
	if (fd < 0) {
		return -errno;
	}
	if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode)) {
		err = -EINVAL;
		close(fd);
		return err;
	}
	do {
		err = flock(fd, exclusive ? LOCK_EX : LOCK_SH);
	} while (err < 0 && errno == EINTR);
	if (err < 0) {
		err = -errno;
		close(fd);
		return err;
	}
	file_operation_active = true;
	return fd;
}

static void file_release(int fd)
{
	close(fd);
	file_operation_active = false;
}

int settings_backend_init(void);

static int settings_file_load(struct settings_store *cs,
			      const struct settings_load_arg *arg);
static int settings_file_save(struct settings_store *cs, const char *name,
			      const char *value, size_t val_len);
static void *settings_file_storage_get(struct settings_store *cs);

static const struct settings_store_itf settings_file_itf = {
	.csi_load = settings_file_load,
	.csi_save = settings_file_save,
	.csi_storage_get = settings_file_storage_get
};

/*
 * Register a file to be a source of configuration.
 */
int settings_file_src(struct settings_file *cf)
{
	if (!cf || !cf->cf_name || !cf->cf_name[0] ||
	    strlen(cf->cf_name) + sizeof(".lock") > SETTINGS_FILE_NAME_MAX) {
		return -EINVAL;
	}
	cf->cf_store.cs_itf = &settings_file_itf;
	settings_src_register(&cf->cf_store);

	return 0;
}

/*
 * Register a file to be a destination of configuration.
 */
int settings_file_dst(struct settings_file *cf)
{
	if (!cf || !cf->cf_name || !cf->cf_name[0] ||
	    strlen(cf->cf_name) + sizeof(".lock") > SETTINGS_FILE_NAME_MAX) {
		return -EINVAL;
	}
	cf->cf_store.cs_itf = &settings_file_itf;
	settings_dst_register(&cf->cf_store);

	return 0;
}

/**
 * @brief Check if there is any duplicate of the current setting
 *
 * This function checks if there is any duplicated data further in the buffer.
 *
 * @param entry_ctx Current entry context
 * @param name      The name of the current entry
 *
 * @retval 0 No duplicates found.
 * @retval 1 Duplicate found.
 * @return A negative errno on a read error.
 */
static int settings_file_check_duplicate(const struct line_entry_ctx *entry_ctx,
					 const char *const name)
{
	struct line_entry_ctx entry2_ctx = *entry_ctx;

	int rc;

	/* Searching the duplicates */
	while ((rc = settings_next_line_ctx(&entry2_ctx)) == 0) {
		char name2[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];
		size_t name2_len;

		if (entry2_ctx.len == 0) {
			break;
		}

		rc = settings_line_name_read(name2, sizeof(name2), &name2_len, &entry2_ctx);
		if (rc) {
			return rc < 0 ? rc : -EBADMSG;
		}
		name2[name2_len] = '\0';

		if (!strcmp(name, name2)) {
			return true;
		}
	}
	return rc < 0 ? rc : 0;
}

static int read_entry_len(const struct line_entry_ctx *entry_ctx, off_t off)
{
	if ((size_t)off >= entry_ctx->len) {
		return 0;
	}
	return entry_ctx->len - off;
}

static int settings_file_load_priv(struct settings_store *cs, line_load_cb cb, void *cb_arg,
				   bool filter_duplicates)
{
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);
	struct settings_file_io file;
	int lines;
	int rc;

	struct line_entry_ctx entry_ctx = {
		.stor_ctx = (void *)&file,
		.seek = 0,
		.len = 0 /* unknown length */
	};

	lines = 0;

	file_init(&file);

	rc = file_open(&file, cf->cf_name, O_RDONLY);
	if (rc != 0) {
		if (rc == -ENOENT) {
			return -ENOENT;
		}

		return rc;
	}

	while (1) {
		char name[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];
		size_t name_len;
		bool pass_entry = true;

		rc = settings_next_line_ctx(&entry_ctx);
		if (rc || entry_ctx.len == 0) {
			break;
		}

		rc = settings_line_name_read(name, sizeof(name), &name_len,
					     &entry_ctx);

		if (rc || name_len == 0) {
			break;
		}
		name[name_len] = '\0';

		if (filter_duplicates) {
			int duplicate = settings_file_check_duplicate(&entry_ctx, name);

			if (duplicate < 0) {
				rc = duplicate;
				break;
			}
			if (!read_entry_len(&entry_ctx, name_len + 1) || duplicate) {
				pass_entry = false;
			}
		}
		/*name, val-read_cb-ctx, val-off*/
		/* take into account '=' separator after the name */
		if (pass_entry) {
			rc = cb(name, (void *)&entry_ctx, name_len + 1, cb_arg);
			if (rc) {
				break;
			}
		}
		lines++;
	}

	int close_rc = file_close(&file);
	cf->cf_lines = lines;

	return rc < 0 ? rc : close_rc;
}

/*
 * Called to load configuration items.
 */
static int settings_file_load_locked(struct settings_store *cs, const struct settings_load_arg *arg)
{
	return settings_file_load_priv(cs,
				       settings_line_load_cb,
				       (void *)arg,
				       true);
}

static int settings_tmpfile(char *dst, const char *src, char *pfx)
{
	int length = snprintf(dst, SETTINGS_FILE_NAME_MAX, "%s%s", src, pfx);

	return length < 0 || length >= SETTINGS_FILE_NAME_MAX ? -ENAMETOOLONG : 0;
}

static int settings_file_create_or_replace(struct settings_file_io *zfp, const char *file_name)
{
	struct settings_file_stat entry;

	if (file_stat(file_name, &entry) == 0) {
		if (entry.type == FILE_REGULAR) {
			if (file_unlink(file_name)) {
				return -EIO;
			}
		} else {
			return -EISDIR;
		}
	}

	return file_open(zfp, file_name, O_CREAT | O_RDWR);
}

/*
 * Try to compress configuration file by keeping unique names only.
 */
static int settings_file_save_and_compress(struct settings_file *cf,
			   const char *name, const char *value,
			   size_t val_len)
{
	int rc, rc2;
	struct settings_file_io rf;
	struct settings_file_io wf;
	char tmp_file[SETTINGS_FILE_NAME_MAX];
	char name1[SETTINGS_FULL_NAME_LEN];
	char name2[SETTINGS_FULL_NAME_LEN];
	struct line_entry_ctx loc1 = {
		.stor_ctx = &rf,
		.seek = 0,
		.len = 0 /* unknown length */
	};

	struct line_entry_ctx loc2;

	struct line_entry_ctx loc3 = {
		.stor_ctx = &wf
	};

	int copy;
	int lines;
	size_t new_name_len;
	size_t val1_off;

	file_init(&rf);
	file_init(&wf);

	if (file_open(&rf, cf->cf_name, O_CREAT | O_RDWR) != 0) {
		return -ENOEXEC;
	}

	rc = settings_tmpfile(tmp_file, cf->cf_name, ".cmp");
	if (rc) {
		file_close(&rf);
		return rc;
	}

	if (settings_file_create_or_replace(&wf, tmp_file)) {
		file_close(&rf);
		return -ENOEXEC;
	}

	lines = 0;
	new_name_len = strlen(name);

	while (1) {
		rc = settings_next_line_ctx(&loc1);

		if (rc) {
			goto end_rolback;
		}
		if (loc1.len == 0) {
			/* try to amend new value to the compressed file */
			break;
		}

		rc = settings_line_name_read(name1, sizeof(name1), &val1_off,
					     &loc1);
		if (rc) {
			goto end_rolback;
		}

		if (val1_off + 1 == loc1.len) {
			/* Lack of a value so the record is a deletion-record */
			/* No sense to copy empty entry from */
			/* the oldest sector */
			continue;
		}

		/* avoid copying value which will be overwritten by new value*/
		if ((val1_off == new_name_len) &&
		    !memcmp(name1, name, val1_off)) {
			continue;
		}

		loc2 = loc1;

		copy = 1;
		while (1) {
			size_t val2_off;

			rc = settings_next_line_ctx(&loc2);

			if (rc) {
				goto end_rolback;
			}
			if (loc2.len == 0) {
				/* try to amend new value to */
				/* the compressed file */
				break;
			}

			rc = settings_line_name_read(name2, sizeof(name2),
						     &val2_off, &loc2);
			if (rc) {
				goto end_rolback;
			}
			if ((val1_off == val2_off) &&
			    !memcmp(name1, name2, val1_off)) {
				copy = 0; /* newer version doesn't exist */
				break;
			}
		}
		if (!copy) {
			continue;
		}

		loc2 = loc1;
		loc2.len += 2;
		loc2.seek -= 2;
		rc = settings_line_entry_copy(&loc3, 0, &loc2, 0, loc2.len);
		if (rc) {
			/* compressed file might be corrupted */
			goto end_rolback;
		}

		lines++;
	}

	/* at last store the new value */
	rc = settings_line_write(name, value, val_len, 0, &loc3);
	if (rc) {
		/* compressed file might be corrupted */
		goto end_rolback;
	}

	rc = file_close(&wf);
	rc2 = file_close(&rf);
	if (rc == 0 && rc2 == 0) {
		rc = file_rename(tmp_file, cf->cf_name);
		if (rc) {
			return rc;
		}
		cf->cf_lines = lines + 1;
	} else {
		if (rc2 == 0) {
			(void)file_unlink(tmp_file);
		}
		return -EIO;
	}
	/*
	 * XXX at settings_file_load(), look for .cmp if actual file does not
	 * exist.
	 */
	return 0;
end_rolback:
	(void)file_close(&wf);
	if (file_close(&rf) == 0) {
		(void)file_unlink(tmp_file);
	}
	return -EIO;

}

static int settings_file_save_priv(struct settings_store *cs, const char *name, const char *value,
				   size_t val_len)
{
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);
	struct line_entry_ctx entry_ctx;
	struct settings_file_io file;
	int rc2;
	int rc;

	if (!name) {
		return -EINVAL;
	}

	file_init(&file);

	if (cf->cf_maxlines && (cf->cf_lines + 1 >= cf->cf_maxlines)) {
		/*
		 * Compress before config file size exceeds
		 * the max number of lines.
		 */
		return settings_file_save_and_compress(cf, name, value,
						       val_len);
	}

	/*
	 * Open the file to add this one value.
	 */
	rc = file_open(&file, cf->cf_name, O_CREAT | O_RDWR);
	if (rc == 0) {
		if (ftruncate(file.fd, file.limit) < 0) {
			rc = -errno;
		} else {
			rc = file_seek(&file, 0, SEEK_END);
		}
		if (rc == 0) {
			entry_ctx.stor_ctx = &file;
			rc = settings_line_write(name, value, val_len, 0,
						  (void *)&entry_ctx);
			if (rc == 0) {
				cf->cf_lines++;
			} else if (ftruncate(file.fd, file.limit) < 0) {
				rc = -errno;
			}
		}

		rc2 = file_close(&file);
		if (rc == 0) {
			rc = rc2;
		}
	}

	return rc;
}


/*
 * Called to save configuration.
 */
static int settings_file_save_locked(struct settings_store *cs, const char *name, const char *value,
				     size_t val_len)
{
	struct settings_line_dup_check_arg cdca;
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);

	if (val_len > 0 && value == NULL) {
		return -EINVAL;
	}

	/*
	 * Check if we're writing the same value again.
	 */
	cdca.name = name;
	cdca.val = (char *)value;
	cdca.is_dup = 0;
	cdca.val_len = val_len;
	int err = settings_file_load_priv(cs, settings_line_dup_check_cb, &cdca, false);

	if (err && err != -ENOENT) {
		return err;
	}
	if (cdca.is_dup == 1) {
		struct settings_file_io file;
		int sync_err;

		file_init(&file);
		sync_err = file_open(&file, cf->cf_name, O_RDONLY);
		if (sync_err) {
			return sync_err;
		}
		sync_err = fsync(file.fd) == 0 ? 0 : -errno;
		int close_err = file_close(&file);

		if (sync_err || close_err) {
			return sync_err ? sync_err : close_err;
		}
		return sync_parent(cf->cf_name);
	}
	return settings_file_save_priv(cs, name, value, val_len);
}

static int read_handler(void *ctx, off_t off, char *buf, size_t *len)
{
	struct line_entry_ctx *entry_ctx = ctx;
	struct settings_file_io *file = entry_ctx->stor_ctx;
	ssize_t r_len;
	int rc;

	/* 0 is reserved for reading the length-field only */
	if (entry_ctx->len != 0) {
		if ((size_t)off >= entry_ctx->len) {
			*len = 0;
			return 0;
		}

		*len = MIN(*len, entry_ctx->len - (size_t)off);
	}

	rc = file_seek(file, entry_ctx->seek + off, SEEK_SET);
	if (rc) {
		goto end;
	}

	r_len = file_read(file, buf, *len);

	if (r_len >= 0) {
		*len = r_len;
		rc = 0;
	} else {
		rc = r_len;
	}
end:
	return rc;
}

static size_t get_len_cb(void *ctx)
{
	struct line_entry_ctx *entry_ctx = ctx;

	return entry_ctx->len;
}

static int write_handler(void *ctx, off_t off, char const *buf, size_t len)
{
	struct line_entry_ctx *entry_ctx = ctx;
	struct settings_file_io *file = entry_ctx->stor_ctx;
	int rc;

	(void)off;
	/* append to file only */
	rc = file_seek(file, 0, SEEK_END);

	if (rc == 0) {
		rc = file_write(file, buf, len);

		if (rc > 0) {
			rc = 0;
		}
	}

	return rc;
}

void settings_mount_file_backend(struct settings_file *cf)
{
	(void)cf;
	settings_line_io_init(read_handler, write_handler, get_len_cb, 1);
}

static int mkdir_if_not_exists(const char *path)
{
	struct settings_file_stat entry;
	int err;

	err = file_stat(path, &entry);
	if (err == -ENOENT) {
		return file_mkdir(path);
	} else if (err) {
		return err;
	}

	if (entry.type != FILE_DIRECTORY) {
		return -EEXIST;
	}

	return 0;
}

static int mkdir_for_file(const char *file_path)
{
	char dir_path[SETTINGS_FILE_NAME_MAX];
	int err;

	if (strlen(file_path) >= sizeof(dir_path)) {
		return -ENAMETOOLONG;
	}
	for (size_t i = 0; file_path[i] != '\0'; i++) {
		if (i > 0 && file_path[i] == '/') {
			dir_path[i] = '\0';

			err = mkdir_if_not_exists(dir_path);
			if (err) {
				return err;
			}
		}

		dir_path[i] = file_path[i];
	}

	return 0;
}

int settings_backend_init(void)
{
	static struct settings_file config_init_settings_file = {
		.cf_name = NULL, .cf_maxlines = SETTINGS_FILE_MAX_LINES};
	int rc;

	config_init_settings_file.cf_name = configured_path;
	rc = mkdir_for_file(config_init_settings_file.cf_name);
	if (rc) {
		return rc;
	}
	rc = settings_file_src(&config_init_settings_file);
	if (rc) {
		return rc;
	}

	rc = settings_file_dst(&config_init_settings_file);
	if (rc) {
		return rc;
	}

	settings_mount_file_backend(&config_init_settings_file);

	/*
	 * Must be called after root FS has been initialized.
	 */
	return 0;
}

static void *settings_file_storage_get(struct settings_store *cs)
{
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);

	return (void *)cf->cf_name;
}

extern bool settings_subsys_initialized;

int settings_file_set_path(const char *path)
{
	if (!path || !path[0] || strlen(path) + sizeof(".lock") > SETTINGS_FILE_NAME_MAX) {
		return -EINVAL;
	}
	int err = settings_ensure_lock();

	if (err) {
		return err;
	}
	settings_lock_take();
	if (settings_subsys_initialized) {
		settings_lock_release();
		return -EBUSY;
	}
	configured_path = path;
	settings_lock_release();
	return 0;
}

static int settings_file_load(struct settings_store *cs, const struct settings_load_arg *arg)
{
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);
	int fd = file_acquire(cf->cf_name, false);
	int err;

	if (fd < 0) {
		return fd;
	}
	err = settings_file_load_locked(cs, arg);
	file_release(fd);
	return err == -ENOENT ? 0 : err;
}

static int settings_file_save(struct settings_store *cs, const char *name, const char *value,
			      size_t val_len)
{
	struct settings_file *cf = CONTAINER_OF(cs, struct settings_file, cf_store);
	int fd, err;

	if (!name || !name[0] || strchr(name, '=') || strlen(name) >= SETTINGS_FULL_NAME_LEN ||
	    (val_len && !value)) {
		return -EINVAL;
	}
	if (val_len > SETTINGS_MAX_VAL_LEN) {
		return -E2BIG;
	}
	fd = file_acquire(cf->cf_name, true);
	if (fd < 0) {
		return fd;
	}
	err = settings_file_save_locked(cs, name, value, val_len);
	file_release(fd);
	return err;
}
