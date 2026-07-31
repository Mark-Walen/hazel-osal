#ifndef OSAL_TEST_CTYPE_H
#define OSAL_TEST_CTYPE_H

static inline int isspace(int character)
{
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

#endif
