// user_auth.c - Handles registration and login securely

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <jansson.h>

#define USERS_FILE "users.json"
#define MAX_USERNAME 32
#define MAX_PASSWORD 64

// Create users.json file if not exists
void init_user_file() {
    FILE* file = fopen(USERS_FILE, "r");
    if (!file) {
        json_t* root = json_object();
        json_dump_file(root, USERS_FILE, JSON_INDENT(2));
        json_decref(root);
    } else {
        fclose(file);
    }
}

// Save a new user with hashed password
int register_user(const char* username, const char* password) {
    json_error_t error;
    json_t* root = json_load_file(USERS_FILE, 0, &error);
    if (!root) {
        fprintf(stderr, "Failed to load %s: %s\n", USERS_FILE, error.text);
        return 0;
    }

    if (json_object_get(root, username)) {
        json_decref(root);
        return -1; // user exists
    }

    char salt[] = "$6$rounds=5000$saltstring$"; // SHA-512
    char* hashed = crypt(password, salt);
    json_object_set_new(root, username, json_string(hashed));
    json_dump_file(root, USERS_FILE, JSON_INDENT(2));
    json_decref(root);
    return 1;
}

// Authenticate user login
int login_user(const char* username, const char* password) {
    json_error_t error;
    json_t* root = json_load_file(USERS_FILE, 0, &error);
    if (!root) {
        fprintf(stderr, "Failed to load %s: %s\n", USERS_FILE, error.text);
        return 0;
    }

    json_t* stored_hash = json_object_get(root, username);
    if (!stored_hash) {
        json_decref(root);
        return -1; // user not found
    }

    const char* hash_str = json_string_value(stored_hash);
    char* input_hash = crypt(password, hash_str);
    int match = strcmp(input_hash, hash_str) == 0;
    json_decref(root);
    return match;
}
