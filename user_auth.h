// user_auth.h – Secure registration / login API
#ifndef USER_AUTH_H
#define USER_AUTH_H

void  init_user_file(void);
int   register_user(const char *username, const char *password);  // 1 → OK, –1 → user exists
int   login_user   (const char *username, const char *password);  // 1 → OK,  0 → wrong

#endif /* USER_AUTH_H */
