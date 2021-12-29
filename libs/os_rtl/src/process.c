#include <os-libc.h>
#include <string.h>
#include <stdarg.h>

int spawnm(const struct proc_header *imagehdr, int prio, const char *arg0, ...) {
    struct proc_attr pattr;
    size_t arglen = 0; // общий размер аргументов: таблица указателей с обязательным терминальным и строки
    int argc = (arg0 == NULL) ? 0 : 1;
    const char *s = arg0;
    va_list args;
    if(argc > 0) {
        arglen += strnlen(s, ARG_MAX - arglen) + 1 + sizeof(char *);
        va_start(args, arg0);
        for(; argc < VA_LIST_ARGS_LIMIT; argc++) {
            if( (s = va_arg(args, char *)) == NULL ) break;
            // считаем общий размер аргументов, учитываем '\0' и место под указатель на строку в таблице
            // применяем защитный метод подсчета с ограничением через strnlen
            arglen += strnlen(s, ARG_MAX - arglen) + 1 + sizeof(char *);
        }
        va_end(args);
    }
    if(argc > 0) {
        // здесь только размер таблицы указателей, учитывая терминальный
        pattr.arglen = sizeof(char *) * argc + sizeof(char *);
    } else {
        pattr.arglen = 0;
    }
    if( (argc == VA_LIST_ARGS_LIMIT) || (arglen > ARG_MAX) ) {
        errno = E2BIG;
        return -1;
    }
    if(s != NULL) {
        errno = EINVAL;
        return -1;
    }
    char *argv_table[argc];

    if(argc > 0) {
        argv_table[0] = (char *)arg0;
        va_start(args, arg0);
        for(int i = 1; i < argc; i++) {
            argv_table[i] = va_arg(args, char *);
        }
        va_end(args);
        argv_table[argc] = NULL; // обязательный терминальный указатель
        pattr.argv = argv_table;
    } else {
        pattr.argv = NULL;
    }
    pattr.pid = -1;
    pattr.tid = -1;
    pattr.prio = prio;
    pattr.argtype = PROC_ARGTYPE_STRING_ARRAY;

    int res;
    if(SYSCALL_ERROR_SET(os_proc_create(imagehdr, &pattr), res)) {
        switch(res) {
        default:
        case ERR:
            errno = ENOEXEC;
            break;
        case ERR_PATHNAME_TOO_LONG:
            errno = ENAMETOOLONG;
            break;
        case ERR_BUSY:
            errno = EBUSY;
            break;
        case ERR_PROC_LIMIT:
        case ERR_THREAD_LIMIT:
        case ERR_NO_MEM:
            errno = ENOMEM;
            break;
        case ERR_ILLEGAL_ARGS:
            errno = EINVAL;
            break;
        }
        return -1;
    }
    return pattr.pid;
}
