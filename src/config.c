#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "jsmn.h"

// helper for jsmn
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static void get_config_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";
    snprintf(path, size, "%s/.chupet-config.json", home);
}

void load_config(AppConfig *config) {
    // default config
    snprintf(config->targetLanguage, sizeof(config->targetLanguage), "English");
    config->provider[0] = '\0';
    config->modelName[0] = '\0';
    config->geminiApiKey[0] = '\0';

    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) {
        fclose(f);
        return;
    }

    char *json = malloc(fsize + 1);
    if (!json) {
        fclose(f);
        return;
    }
    fread(json, 1, fsize, f);
    fclose(f);
    json[fsize] = '\0';

    jsmn_parser p;
    jsmntok_t t[128];
    jsmn_init(&p);
    int r = jsmn_parse(&p, json, strlen(json), t, sizeof(t) / sizeof(t[0]));
    
    if (r < 0 || t[0].type != JSMN_OBJECT) {
        free(json);
        return;
    }

    for (int i = 1; i < r; i++) {
        if (jsoneq(json, &t[i], "targetLanguage") == 0) {
            snprintf(config->targetLanguage, sizeof(config->targetLanguage), "%.*s", t[i+1].end - t[i+1].start, json + t[i+1].start);
            i++;
        } else if (jsoneq(json, &t[i], "provider") == 0) {
            snprintf(config->provider, sizeof(config->provider), "%.*s", t[i+1].end - t[i+1].start, json + t[i+1].start);
            i++;
        } else if (jsoneq(json, &t[i], "modelName") == 0) {
            snprintf(config->modelName, sizeof(config->modelName), "%.*s", t[i+1].end - t[i+1].start, json + t[i+1].start);
            i++;
        } else if (jsoneq(json, &t[i], "geminiApiKey") == 0) {
            snprintf(config->geminiApiKey, sizeof(config->geminiApiKey), "%.*s", t[i+1].end - t[i+1].start, json + t[i+1].start);
            i++;
        }
    }
    free(json);
}

void save_config(const AppConfig *config) {
    char path[512];
    get_config_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"targetLanguage\": \"%s\",\n", config->targetLanguage);
    fprintf(f, "  \"provider\": \"%s\",\n", config->provider);
    fprintf(f, "  \"modelName\": \"%s\",\n", config->modelName);
    fprintf(f, "  \"geminiApiKey\": \"%s\"\n", config->geminiApiKey);
    fprintf(f, "}\n");
    
    fclose(f);
}
