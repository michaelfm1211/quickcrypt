#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>
#include <sodium.h>

#define HELP \
    "QuickCrypt v0.1.0 (for libsodium v1.0.18)\n" \
    "usage: qc [-h] [-b] [-k keyfile] [-s salt] [-p password] [-g] [-e]" \
            " [-d]\n" \
    "  -h\t\tPrints this help message\n" \
    "  -k keyfile\tSpecify the path to a key file (and use if -g or -p is" \
            " not specified\n" \
    "  -g\t\tGenerate a key, write result in the key file, and use\n" \
    "  -s salt\tSpecify a salt to use when deriving keys from passwords.\n" \
    "  -p password\tDerive a key from a password to use. Salting with -s is" \
            " reccomended\n" \
    "  -e\t\tEncrypt stdin and send ciphertext into stdout\n" \
    "  -d\t\tDecrypt stdin and send plaintext into stdout\n"

#define ERR_NO_KEY "error: no key was provided\n"
#define ERR_KEY_TWICE "error: a key was provided twice\n"
#define ERR_KEY_EXISTS "error: %s already exists\n"
#define ERR_SALT_TOO_LONG "error: the salt provided is longer than %d\n"
#define ERR_NO_MEM "error: out of memory\n"

#define CHUNK_SIZE 4096

bool salt_exists = false;
uint8_t pass_salt[crypto_pwhash_SALTBYTES];

const char *key_path = NULL;
bool key_exists = false, key_base64 = false;
uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];

static void panic(const char *str, ...) {
    va_list vargs;
    va_start(vargs, str);
    if (errno != 0)
        perror(str);
    else
        vfprintf(stderr, str, vargs);
    va_end(vargs);
    exit(1);
}

void read_key() {
    if (key_path == NULL)
        panic(ERR_NO_KEY);

    FILE *keyfile = fopen(key_path, "r");
    if (keyfile == NULL) {
        panic(key_path);
    }

    fread(key, crypto_secretstream_xchacha20poly1305_KEYBYTES, 1, keyfile);
    key_exists = true;
}

void gen() {
    if (key_path == NULL)
        panic(ERR_NO_KEY);
    else if (key_exists)
        panic(ERR_KEY_TWICE);
    else if (access(key_path, F_OK) == 0)
        panic(ERR_KEY_EXISTS, key_path);

    FILE *key_file = fopen(key_path, "w");
    if (key_file == NULL)
        panic(key_path);

    crypto_secretstream_xchacha20poly1305_keygen(key);
    
    fwrite(key, crypto_secretstream_xchacha20poly1305_KEYBYTES, 1, key_file);
    key_exists = true;
}

void salt(const char *str) {
    size_t str_len = strlen(str) * sizeof(char);
    if (str_len > crypto_pwhash_SALTBYTES)
        panic(ERR_SALT_TOO_LONG, crypto_pwhash_SALTBYTES);
    memcpy(pass_salt, str, str_len);
    salt_exists = true;
}

void pass(const char *password) {
    if (key_exists)
        panic(ERR_KEY_TWICE);

    if (!salt_exists)
        memset(pass_salt, 0, crypto_pwhash_SALTBYTES);

    if (crypto_pwhash(key, sizeof(key), password, strlen(password), pass_salt,
             crypto_pwhash_OPSLIMIT_INTERACTIVE,
             crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) !=
             0)
        panic(ERR_NO_MEM);
    key_exists = true;
}

int enc() {
    if (!key_exists)
        read_key();

    uint8_t buf_in[CHUNK_SIZE];
    uint8_t buf_out[CHUNK_SIZE +
        crypto_secretstream_xchacha20poly1305_ABYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_state st;
    uint64_t out_len;
	int eof;

    crypto_secretstream_xchacha20poly1305_init_push(&st, header, key);
    fwrite(header, 1, sizeof header, stdout);

    do {
        size_t rlen = fread(buf_in, 1, sizeof buf_in, stdin);
        eof = feof(stdin);
        uint8_t tag = eof ? crypto_secretstream_xchacha20poly1305_TAG_FINAL :
            0;
        crypto_secretstream_xchacha20poly1305_push(&st, buf_out, &out_len,
                buf_in, rlen, NULL, 0, tag);
        fwrite(buf_out, 1, (size_t) out_len, stdout);
    } while (!eof);

    return 0;
}

int dec() {
    if (!key_exists)
        read_key();

    uint8_t buf_in[CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
    uint8_t buf_out[CHUNK_SIZE];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_state st;
    unsigned long long out_len;
    int eof;
    int ret = -1;
    uint8_t tag;

    fread(header, 1, sizeof header, stdin);
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 
            0) {
        goto ret;
    }

    do {
        size_t rlen = fread(buf_in, 1, sizeof buf_in, stdin);
        eof = feof(stdin);
        if (crypto_secretstream_xchacha20poly1305_pull(&st, buf_out, &out_len,
                    &tag, buf_in, rlen, NULL, 0) != 0) {
            goto ret;
        } else if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL &&
                !eof) {
            goto ret;
        }
        fwrite(buf_out, 1, (size_t)out_len, stdout);
    } while (!eof);

    ret = 0;
ret:
    return ret;
}

int main(int argc, char **argv) {
    if (argc < 2)
usage:  panic(HELP);

    if (sodium_init() != 0)
        return 1;

    char ch;
    while ((ch = getopt(argc, argv, "bk:s:p:gedh")) != -1) {
        switch (ch) {
        case 'b':
            key_base64 = true;
        case 'k':
            key_path = optarg;
            break;
        case 'g':
            gen();
            break;
        case 's':
            salt(optarg);
            break;
        case 'p':
            pass(optarg);
            break;
        case 'e':
            return enc();
        case 'd':
            return dec();
        case 'h':
        case '?':
        default:
            goto usage;
        }
    }

    return 0;
}

