uint8_t *read_entire_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) < 0) {
        fprintf(stderr, "Failed to read file %s\n", path);
        exit(1);
    }

    long file_size = ftell(f);
    assert(file_size > 0);

    rewind(f);

    uint8_t *file_data = malloc(file_size);
    assert(file_data);

    size_t bytes_read = fread(file_data, 1, file_size, f);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Could not read entire file\n");
        exit(1);
    }

    if (size != NULL) {
        *size = bytes_read;
    }
    return file_data;
}
