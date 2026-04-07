### chupet

POSIX style CLI translator

```bash
chupet -o Japanese -t English "カルチャーを文字で話す" >> babel.txt
```

Currently supports `ollama` and `Gemini API`.

**Setup**

```bash
chupet config provider ollama
chupet config model translategemma:latest
```

or

```bash
chupet config provider gemini
chupet config model gemini-3.1-flash-lite-preview
chupet config key YOUR-API-KEY
```

**Usage**

`-o` or `--origin` for original language

`-t` or `--target` for target language

`-h` or `--help` to print help

You can configure the default original and target languages:

```bash
chupet config origin auto
chupet config target English
```

**Examples**

```bash
chupet -t French "The quick brown fox jumps over the lazy dog."
chupet "我们生前与时代合影
挂在长桌尽头"
chupet < file.txt
cat file.txt | chupet > output.txt
```

<br>

Licensed under MIT.
