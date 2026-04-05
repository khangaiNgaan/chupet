#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <locale.h>

#include "config.h"
#include "http_client.h"
#include "jsmn.h"
#include "linenoise.h"

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

void process_translation(const char *text) {
    if (!text || strlen(text) == 0) return;
    
    printf("\nTranslating... ");
    fflush(stdout);

    char prompt[8192];
    snprintf(prompt, sizeof(prompt), 
             "You are a professional translator. Translate the following text to %s. Only output the translation result, no explanation.\n\nText: %s", 
             config.targetLanguage, text);
             
    char *escaped_prompt = escape_json(prompt);
    
    char *post_data = NULL;
    char url[1024];
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (strcmp(config.provider, "gemini") == 0) {
        snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s", config.modelName, config.geminiApiKey);
        
        size_t post_len = strlen(escaped_prompt) + 256;
        post_data = malloc(post_len);
        if (post_data) {
            snprintf(post_data, post_len, "{\"contents\": [{\"parts\": [{\"text\": \"%s\"}]}]}", escaped_prompt);
        }
    } else { // ollama
        snprintf(url, sizeof(url), "http://localhost:11434/api/generate");
        size_t post_len = strlen(escaped_prompt) + strlen(config.modelName) + 256;
        post_data = malloc(post_len);
        if (post_data) {
            snprintf(post_data, post_len, "{\"model\": \"%s\", \"prompt\": \"%s\", \"stream\": false}", config.modelName, escaped_prompt);
        }
    }
    
    HttpResponse response;
    http_response_init(&response);
    
    if (post_data) {
        int res = http_post(url, headers, post_data, &response);
        printf("\r\033[K"); // clear the "Translating... " line
        
        if (res == 0 && response.data) {
            jsmn_parser p;
            jsmntok_t t[4096]; 
            jsmn_init(&p);
            int r = jsmn_parse(&p, response.data, strlen(response.data), t, sizeof(t) / sizeof(t[0]));
            
            if (r < 0) {
                printf("Error: Failed to parse JSON response.\n%s\n", response.data);
            } else {
                bool found = false;
                for (int i = 1; i < r; i++) {
                    if (t[i].type == JSMN_STRING) {
                        int len = t[i].end - t[i].start;
                        const char *str = response.data + t[i].start;
                        if ((strncmp(str, "text", len) == 0 && len == 4) ||
                            (strncmp(str, "response", len) == 0 && len == 8)) {
                            
                            if (i + 1 < r && t[i+1].type == JSMN_STRING) {
                                int val_len = t[i+1].end - t[i+1].start;
                                const char *val_str = response.data + t[i+1].start;
                                
                                for (int j = 0; j < val_len; j++) {
                                    if (val_str[j] == '\\' && j + 1 < val_len) {
                                        j++;
                                        if (val_str[j] == 'n') putchar('\n');
                                        else if (val_str[j] == 't') putchar('\t');
                                        else if (val_str[j] == '"') putchar('"');
                                        else if (val_str[j] == '\\') putchar('\\');
                                        else putchar(val_str[j]);
                                    } else {
                                        putchar(val_str[j]);
                                    }
                                }
                                printf("\n\n");
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found) {
                    printf("Error: Unexpected response format.\n%s\n", response.data);
                }
            }
        } else {
            printf("Error: HTTP request failed.\n");
        }
        free(post_data);
    }
    
    free(escaped_prompt);
    http_response_free(&response);
    curl_slist_free_all(headers);
}

void print_usage() {
    printf("Usage:\n");
    printf("  chupet set <language>\n");
    printf("  chupet to <language>\n");
    printf("  chupet config provider <ollama|gemini>\n");
    printf("  chupet config model <model_name>\n");
    printf("  chupet config key <api_key>\n");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    load_config(&config);

    if (argc > 1) {
        if (strcmp(argv[1], "set") == 0 && argc >= 3) {
            char lang[128] = {0};
            for (int i = 2; i < argc; i++) {
                size_t current_len = strlen(lang);
                snprintf(lang + current_len, sizeof(lang) - current_len, "%s%s", (i > 2 ? " " : ""), argv[i]);
            }
            snprintf(config.targetLanguage, sizeof(config.targetLanguage), "%s", lang);
            save_config(&config);
            printf("Target language set to: %s\n", lang);
            return 0;
        } else if (strcmp(argv[1], "to") == 0 && argc >= 3) {
            char lang[128] = {0};
            for (int i = 2; i < argc; i++) {
                size_t current_len = strlen(lang);
                snprintf(lang + current_len, sizeof(lang) - current_len, "%s%s", (i > 2 ? " " : ""), argv[i]);
            }
            snprintf(config.targetLanguage, sizeof(config.targetLanguage), "%s", lang);
        } else if (strcmp(argv[1], "config") == 0 && argc == 4) {
            if (strcmp(argv[2], "provider") == 0) {
                snprintf(config.provider, sizeof(config.provider), "%s", argv[3]);
                save_config(&config);
                printf("Provider set to: %s\n", argv[3]);
            } else if (strcmp(argv[2], "model") == 0) {
                snprintf(config.modelName, sizeof(config.modelName), "%s", argv[3]);
                save_config(&config);
                printf("Model set to: %s\n", argv[3]);
            } else if (strcmp(argv[2], "key") == 0) {
                snprintf(config.geminiApiKey, sizeof(config.geminiApiKey), "%s", argv[3]);
                save_config(&config);
                printf("Gemini API key updated.\n");
            } else {
                print_usage();
                return 1;
            }
            return 0;
        } else {
            print_usage();
            return 1;
        }
    }

    if (strlen(config.provider) == 0 || strlen(config.modelName) == 0 ||
        (strcmp(config.provider, "gemini") == 0 && strlen(config.geminiApiKey) == 0)) {
        printf("Error: Configuration required.\n\n");
        print_usage();
        return 1;
    }

    printf("Chupet Translator (c) 2026 caffeine-Ink\n");
    printf("Target: %s | Provider: %s | Model: %s\n", config.targetLanguage, config.provider, config.modelName);
    printf("Enter text to translate (use \"\"\" for multiline). Press Ctrl+C or Ctrl+D to exit.\n");

    char *line;
    char inputBuffer[16384] = {0};
    bool isMultiline = false;
    char prompt_str[256];
    
    linenoiseHistorySetMaxLen(100);

    while (1) {
        if (!isMultiline) {
            snprintf(prompt_str, sizeof(prompt_str), "chupet (%s) > ", config.targetLanguage);
            line = linenoise(prompt_str);
        } else {
            line = linenoise("... ");
        }

        if (line == NULL) break; // Ctrl+C or Ctrl+D

        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (!isMultiline && strncmp(trimmed, "\"\"\"", 3) == 0) {
            isMultiline = true;
            inputBuffer[0] = '\0';
            size_t t_len = strlen(trimmed);
            if (t_len >= 6 && strcmp(trimmed + t_len - 3, "\"\"\"") == 0 && strcmp(trimmed, "\"\"\"") != 0) {
                isMultiline = false;
                size_t content_len = t_len - 6;
                if (content_len >= sizeof(inputBuffer)) content_len = sizeof(inputBuffer) - 1;
                memcpy(inputBuffer, trimmed + 3, content_len);
                inputBuffer[content_len] = '\0';
                process_translation(inputBuffer);
            } else {
                snprintf(inputBuffer, sizeof(inputBuffer), "%s\n", trimmed + 3);
            }
        } else if (isMultiline) {
            size_t t_len = strlen(trimmed);
            if (t_len >= 3 && strcmp(trimmed + t_len - 3, "\"\"\"") == 0) {
                isMultiline = false;
                size_t current_buf_len = strlen(inputBuffer);
                size_t line_to_add = strlen(line) - 3;
                if (current_buf_len + line_to_add < sizeof(inputBuffer)) {
                    memcpy(inputBuffer + current_buf_len, line, line_to_add);
                    inputBuffer[current_buf_len + line_to_add] = '\0';
                }
                process_translation(inputBuffer);
                inputBuffer[0] = '\0';
            } else {
                size_t current_buf_len = strlen(inputBuffer);
                size_t line_to_add = strlen(line);
                if (current_buf_len + line_to_add + 2 < sizeof(inputBuffer)) {
                    strcat(inputBuffer, line);
                    strcat(inputBuffer, "\n");
                }
            }
        } else if (strlen(trimmed) > 0) {
            linenoiseHistoryAdd(line);
            process_translation(trimmed);
        }

        linenoiseFree(line);
    }

    printf("\nGoodbye!\n");
    return 0;
}
