#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "http_client.h"

void http_response_init(HttpResponse *response) {
    response->data = NULL;
    response->size = 0;
}

void http_response_free(HttpResponse *response) {
    if (response->data) {
        free(response->data);
        response->data = NULL;
    }
    response->size = 0;
}

int http_post(const char *url, struct curl_slist *headers, const char *post_data, HttpResponse *response) {
    // We will build a curl command line string
    // Example: curl -s -X POST -H "Content-Type: application/json" -d 'data' 'url'
    
    // For simplicity and maximum compatibility, we use a temporary file for post data 
    // to avoid shell escaping issues with large JSON payloads.
    char tmp_file[] = "/tmp/chupet_post_XXXXXX";
    int fd = mkstemp(tmp_file);
    if (fd == -1) {
        // Fallback for systems without /tmp or mkstemp (like Windows)
        strcpy(tmp_file, "chupet_payload.tmp");
    } else {
        close(fd);
    }

    FILE *f = fopen(tmp_file, "w");
    if (!f) return -1;
    fputs(post_data, f);
    fclose(f);

    char cmd[16384];
    // Construct command: curl -s -X POST -H "..." --data-binary @tmp_file "url"
    snprintf(cmd, sizeof(cmd), "curl -s -X POST ");
    
    struct curl_slist *h = headers;
    while (h) {
        strcat(cmd, "-H \"");
        strcat(cmd, h->data);
        strcat(cmd, "\" ");
        h = h->next;
    }

    strcat(cmd, "--data-binary @");
    strcat(cmd, tmp_file);
    strcat(cmd, " \"");
    strcat(cmd, url);
    strcat(cmd, "\"");

    // Execute and read output
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        remove(tmp_file);
        return -1;
    }

    char buffer[4096];
    size_t total_read = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);
        char *ptr = realloc(response->data, total_read + len + 1);
        if (!ptr) {
            pclose(fp);
            remove(tmp_file);
            return -1;
        }
        response->data = ptr;
        memcpy(response->data + total_read, buffer, len);
        total_read += len;
        response->data[total_read] = '\0';
    }

    int status = pclose(fp);
    remove(tmp_file);
    response->size = total_read;

    return (status == 0) ? 0 : -1;
}
