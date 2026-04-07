#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#include "config.h"
#include "http_client.h"
#include "jsmn.h"

AppConfig config;

// Escape string for JSON
char *escape_json(const char *input) {
    size_t len = strlen(input);
    char *escaped = malloc(len * 2 + 1); 
    if (!escaped) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '"': escaped[j++] = '\\'; escaped[j++] = '"'; break;
            case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
            case '\b': escaped[j++] = '\\'; escaped[j++] = 'b'; break;
            case '\f': escaped[j++] = '\\'; escaped[j++] = 'f'; break;
            case '\n': escaped[j++] = '\\'; escaped[j++] = 'n'; break;
            case '\r': escaped[j++] = '\\'; escaped[j++] = 'r'; break;
            case '\t': escaped[j++] = '\\'; escaped[j++] = 't'; break;
            default: escaped[j++] = input[i]; break;
        }
    }
    escaped[j] = '\0';
    return escaped;
}

void process_translation(const char *text, const char *origin, const char *target) {
    if (!text || strlen(text) == 0) return;
    
    char prompt[16384];
    snprintf(prompt, sizeof(prompt), 
             "You are a professional translator. Translate the following text from %s to %s. Only output the translation result, no explanation. No surplus line break in the end of output. Don't translate the `Text:`.\n\nText: %s", 
             origin, target, text);
             
    char *escaped_prompt = escape_json(prompt);
    char *post_data = NULL;
    char url[1024];
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (strcmp(config.provider, "gemini") == 0) {
        snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s", config.modelName, config.geminiApiKey);
        size_t post_len = strlen(escaped_prompt) + 256;
        post_data = malloc(post_len);
        if (post_data) snprintf(post_data, post_len, "{\"contents\": [{\"parts\": [{\"text\": \"%s\"}]}]}", escaped_prompt);
    } else { // ollama
        snprintf(url, sizeof(url), "http://localhost:11434/api/generate");
        size_t post_len = strlen(escaped_prompt) + strlen(config.modelName) + 256;
        post_data = malloc(post_len);
        if (post_data) snprintf(post_data, post_len, "{\"model\": \"%s\", \"prompt\": \"%s\", \"stream\": false}", config.modelName, escaped_prompt);
    }
    
    HttpResponse response;
    http_response_init(&response);
    
    if (post_data) {
        if (http_post(url, headers, post_data, &response) == 0 && response.data) {
            jsmn_parser p;
            jsmntok_t t[4096]; 
            jsmn_init(&p);
            int r = jsmn_parse(&p, response.data, strlen(response.data), t, sizeof(t) / sizeof(t[0]));
            if (r > 0) {
                for (int i = 1; i < r; i++) {
                    if (t[i].type == JSMN_STRING) {
                        int len = t[i].end - t[i].start;
                        const char *str = response.data + t[i].start;
                        if ((strncmp(str, "text", len) == 0 && len == 4) || (strncmp(str, "response", len) == 0 && len == 8)) {
                            if (i + 1 < r && t[i+1].type == JSMN_STRING) {
                                int val_len = t[i+1].end - t[i+1].start;
                                const char *val_str = response.data + t[i+1].start;
                                for (int k = 0; k < val_len; k++) {
                                    if (val_str[k] == '\\' && k + 1 < val_len) {
                                        k++;
                                        if (val_str[k] == 'n') putchar('\n');
                                        else if (val_str[k] == 't') putchar('\t');
                                        else if (val_str[k] == '"') putchar('"');
                                        else if (val_str[k] == '\\') putchar('\\');
                                        else putchar(val_str[k]);
                                    } else putchar(val_str[k]);
                                }
                                putchar('\n');
                                break;
                            }
                        }
                    }
                }
            }
        }
        free(post_data);
    }
    
    free(escaped_prompt);
    http_response_free(&response);
    curl_slist_free_all(headers);
}

void print_help() {
    printf("USAGE: chupet [options] [text]\n\n");
    printf("chupet v0.2.0 (c) 2026 caffeine-Ink\n\n");
    printf("OPTIONS:\n");
    printf("  -o, --origin <lang>    original language (config: %s)\n", config.originLanguage);
    printf("  -t, --target <lang>    target language   (config: %s)\n", config.targetLanguage);
    printf("  -h, --help             show this help\n");
    printf("COMMANDS:\n");
    printf("  config <key> <value>   update configuration\n");
    printf("EXAMPLES:\n");
    printf("  chupet -o English -t Japanese \"The quick brown fox jumps over the lazy dog.\"\n");
    printf("  chupet < file.txt\n");
    printf("  cat file.txt | chupet -t Chinese > output.txt\n");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    load_config(&config);

    // handle config command
    if (argc >= 2 && strcmp(argv[1], "config") == 0) {
        if (argc == 2) {
            printf("CURRENT CONFIG:\n");
            printf("  provider: %s\n", strlen(config.provider) > 0 ? config.provider : "<not set>");
            printf("  model   : %s\n", strlen(config.modelName) > 0 ? config.modelName : "<not set>");
            printf("  key     : %s\n", strlen(config.geminiApiKey) > 0 ? "<configured>" : "<not set>");
            printf("  origin  : %s\n", config.originLanguage);
            printf("  target  : %s\n", config.targetLanguage);
            return 0;
        }
        if (argc >= 4) {
            if (strcmp(argv[2], "provider") == 0) {
                snprintf(config.provider, sizeof(config.provider), "%s", argv[3]);
                printf("Provider is set to: %s\n", argv[3]);
            } else if (strcmp(argv[2], "model") == 0) {
                snprintf(config.modelName, sizeof(config.modelName), "%s", argv[3]);
                printf("Model is set to: %s\n", argv[3]);
            } else if (strcmp(argv[2], "key") == 0) {
                snprintf(config.geminiApiKey, sizeof(config.geminiApiKey), "%s", argv[3]);
                printf("Gemini API key updated.\n");
            } else if (strcmp(argv[2], "origin") == 0) {
                snprintf(config.originLanguage, sizeof(config.originLanguage), "%s", argv[3]);
                printf("Default original language is set to: %s\n", argv[3]);
            } else if (strcmp(argv[2], "target") == 0) {
                snprintf(config.targetLanguage, sizeof(config.targetLanguage), "%s", argv[3]);
                printf("Default target language is set to: %s\n", argv[3]);
            } else { 
                printf("unknown config key: %s\n", argv[2]); return 1; 
            }
            save_config(&config);
            return 0;
        }
        printf("usage: chupet config <key> <value>\n");
        return 1;
    }

    char *origin_override = NULL;
    char *target_override = NULL;
    char *text_arg = NULL;

    // parse options
    int opt;
    extern char *optarg;
    extern int optind;
    while ((opt = getopt(argc, argv, "o:t:h")) != -1) {
        switch (opt) {
            case 'o': origin_override = optarg; break;
            case 't': target_override = optarg; break;
            case 'h': print_help(); return 0;
            default: print_help(); return 1;
        }
    }

    if (optind < argc) {
        text_arg = argv[optind];
    }

    // check config
    if (strlen(config.provider) == 0 || strlen(config.modelName) == 0 ||
        (strcmp(config.provider, "gemini") == 0 && strlen(config.geminiApiKey) == 0)) {
        printf("configuration required. run 'chupet -h' for help.\n");
        return 1;
    }

    const char *origin = origin_override ? origin_override : config.originLanguage;
    const char *target = target_override ? target_override : config.targetLanguage;

    if (text_arg) {
        process_translation(text_arg, origin, target);
    } else {
        // read from stdin (pipe support)
        char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf) - 1, stdin);
        if (n > 0) {
            buf[n] = '\0';
            process_translation(buf, origin, target);
        }
    }

    return 0;
}
