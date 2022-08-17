# qc
QuickCrypt (`qc`) is a simple tool for encrypting and decrypting data with libsodium. Only the default ciphers provided by libsodium are supported, and data is fed into libsodium as-is without any decoding (ie: from base64 or hex). The goal is to be an easy to use interface to libsodium from the command line.

# How to use
qc can do three things, encrypt data (with `-e`), decrypt data (with `-d`), or generate keys (with `-g`). The basic syntax of a qc command is `qc key_options... mode`, where `options...` is one or more options that tell qc about the key, and `mode` is `-e`, `-d`, or `-g`. All qc modes require a key and there are four ways to give qc a key. They are:
- Specify a key file with `-k key_file`,
- Generate a new key file with `-k key_file -g`,
- Derive a key from a salt and a password `-s salt -p password`, or
- Derive a key from a password `-p password` (always prefer using a salt).

Note how `-g` can be both a key option and a mode. This is useful if you want to generate a key and encrypt data with that key all in one go. If you wish to utilize this, it is important to know that qc reads options in order. For example, `qc -k mykey -g -e` will first generate a new key then encrypt data, but `qc -k mykey -e -g` will attempt to encrypt data with `mykey` then generate `mykey`, something which makes no sense and will result in an error.

qc reads data from standard input and writes to standard output, so there is no need to specify input and output files with options. If you wanted to encrypt `sensitive_note.txt` with key `mykey` and store the output in `secured.enc`, then all you have to do is `qc -k mykey -e < sensitive_note.txt > secured.enc`
