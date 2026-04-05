#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char targetLanguage[128];
    char provider[32];
    char modelName[128]; // 通用的模型名称字段
    char geminiApiKey[256];
} AppConfig;

void load_config(AppConfig *config);
void save_config(const AppConfig *config);

#endif
