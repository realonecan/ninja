// user_auth.c – File-based user DB with salted SHA-512 hashes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <jansson.h>

#define USERS_FILE     "users.json"
#define MAX_USERNAME   32
#define MAX_PASSWORD   64
#define SALT_BYTES     16          // 128-bit salt

static void random_salt(char buf[SALT_BYTES + 1])
{
    FILE *rf = fopen("/dev/urandom", "rb");
    fread(buf, 1, SALT_BYTES, rf);
    fclose(rf);

    const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789./";
    for (size_t i = 0; i < SALT_BYTES; ++i)
        buf[i] = alphabet[(unsigned char)buf[i] % (sizeof(alphabet) - 1)];
    buf[SALT_BYTES] = '\0';
}

void init_user_file(void)
{
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) {
        json_t *root = json_object();
        json_dump_file(root, USERS_FILE, JSON_INDENT(2));
        json_decref(root);
    } else {
        fclose(f);
    }
}

int register_user(const char *username, const char *password)
{
    json_error_t err;
    json_t *root = json_load_file(USERS_FILE, 0, &err);
    if (!root) root = json_object();

    if (json_object_get(root, username)) {         // user exists
        json_decref(root); return -1;
    }

    char salt_str[SALT_BYTES + 1];
    random_salt(salt_str);
    char salt_fmt[64];
    snprintf(salt_fmt, sizeof(salt_fmt),
             "$6$rounds=100000$%s$", salt_str);     // SHA-512 - 100 k rounds

    char *hash = crypt(password, salt_fmt);
    json_object_set_new(root, username, json_string(hash));

    json_dump_file(root, USERS_FILE, JSON_INDENT(2));
    json_decref(root);
    return 1;
}

int login_user(const char *username, const char *password)
{
    json_error_t err;
    json_t *root = json_load_file(USERS_FILE, 0, &err);
    if (!root) return 0;

    json_t *stored = json_object_get(root, username);
    if (!stored) { json_decref(root); return 0; }

    const char *stored_hash = json_string_value(stored);
    char *calc = crypt(password, stored_hash);
    int ok = strcmp(calc, stored_hash) == 0;

    json_decref(root);
    return ok;
}
