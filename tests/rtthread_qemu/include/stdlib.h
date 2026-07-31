#ifndef OSAL_TEST_STDLIB_H
#define OSAL_TEST_STDLIB_H

void abort(void) __attribute__((noreturn));
long long strtoll(const char *text, char **end, int base);
unsigned long long strtoull(const char *text, char **end, int base);
long double strtold(const char *text, char **end);
double strtod(const char *text, char **end);
float strtof(const char *text, char **end);

#endif
