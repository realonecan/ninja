// user_auth.h - Header for secure user auth logic

#ifndef USER_AUTH_H
#define USER_AUTH_H

void init_user_file();
int register_user(const char* username, const char* password);
int login_user(const char* username, const char* password);

#endif // USER_AUTH_H
