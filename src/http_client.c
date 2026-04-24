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
    // create temporary file for payload
    char tmp_file[256];
    snprintf(tmp_file, sizeof(tmp_file), "/tmp/chupet_post_XXXXXX");
    int fd = mkstemp(tmp_file);
    if (fd != -1) {
        close(fd);
    } else {
        snprintf(tmp_file, sizeof(tmp_file), "chupet_payload.tmp");
    }

    FILE *f = fopen(tmp_file, "w");
    if (!f) return -1;
    fputs(post_data, f);
    fclose(f);

    // build curl command string
    char cmd[16384];
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

    // execute command and read output
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
