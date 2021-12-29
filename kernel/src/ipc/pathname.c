#include "pathname.h"
#include "common\namespace.h"
#include <config.h>
#include <string.h>

//static namespace_t kernel_namespace;
//static namespace_t public_namespace;
//static namespace_t protected_namespace;

static namespace_t namespace;

//static bool is_process_folder (char *pathname)
//{
//    return memcmp(pathname, PROCESS_HOME, strlen(PROCESS_HOME)) == 0 ? true : false;
//}
//
//
//static inline bool is_kernel_folder (char *pathname)
//{
//    return memcmp(pathname, KERNEL_HOME, strlen(KERNEL_HOME)) == 0 ? true : false;
//}


//static namespace_t* get_namespace (char **pathname)
//{
//    // TODO - дубовая реализация, переделать на нормальный поиск (может быть по буквам через дерево?)
//
//    if (*pathname == '$') {
//        pathname++;
//        if (is_kernel_folder(pathname)) {
//            if (strcmp(pathname, LOGGER_CHANNEL) == 0) {
//                ;
//            } else if (strcmp(pathname, LOGGER_CHANNEL) == 0) {
//                ;
//            } else {
//                *pathname = '\0';
//                return false;
//            }
//            return true;
//        } else if (is_process_folder(pathname)) {
//            if (strcmp(pathname, SIGNAL_CHANNEL) == 0) {
//                ;
//            } else {
//                *pathname = '\0';
//                return false;
//            }
//            return false;
//        }
//        *pathname = '\0';
//    }
//
//    return &namespace;
//}


void pathname_init ()
{
    ns_init(&namespace);
}


pathname_t* create_pathname (char *pathname)
{
    if (!pathname)
        return NULL;

    //namespace_t *namespace = get_namespace(pathname);
    //char *fpathname = format(pathname);

    namespace_node_t *node = NULL;
    int err = ns_node_create_and_lock(&namespace, pathname, &node, 0);
    //kfree(fpathname);
    if (err != OK) {
        return NULL;
    }
    ns_node_set_value(node, 0);
    return (pathname_t*)node;
}


pathname_t* lock_pathname (char *pathname, char **suffix)
{
    if (!pathname)
        return NULL;

    namespace_node_t *node = NULL;
    int err = ns_node_search_and_lock(&namespace, pathname, suffix, &node);
    if (err != OK) {
        return NULL;
    }
    return (pathname_t*)node;
}


void unlock_pathname (pathname_t *pathname)
{
    ns_node_unlock(&namespace, (namespace_node_t *)pathname);
}


void delete_pathname (pathname_t *pathname)
{
    namespace_node_t *node = (namespace_node_t *)pathname;
    if (!node)
        return;

    ns_node_delete(&namespace, node, 1);
}


void set_pathname_ref (pathname_t *pathname, void *ref)
{
    ns_node_set_ref((namespace_node_t *)pathname, ref);
}


void* get_pathname_ref (pathname_t *pathname)
{
    return ns_node_get_ref((namespace_node_t *)pathname);
}
