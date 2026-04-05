#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Mocking curl_slist to avoid dependency on curl headers
struct curl_slist {
    char *data;
    struct curl_slist *next;
};

typedef struct {
    char *data;
    size_t size;
} HttpResponse;

void http_response_init(HttpResponse *response);
void http_response_free(HttpResponse *response);
int http_post(const char *url, struct curl_slist *headers, const char *post_data, HttpResponse *response);

// Helper functions for list management (previously provided by libcurl)
static inline struct curl_slist* curl_slist_append(struct curl_slist *list, const char *str) {
    struct curl_slist *new_node = malloc(sizeof(struct curl_slist));
    new_node->data = strdup(str);
    new_node->next = NULL;
    if (!list) return new_node;
    struct curl_slist *cur = list;
    while (cur->next) cur = cur->next;
    cur->next = new_node;
    return list;
}

static inline void curl_slist_free_all(struct curl_slist *list) {
    while (list) {
        struct curl_slist *next = list->next;
        free(list->data);
        free(list);
        list = next;
    }
}

#endif
