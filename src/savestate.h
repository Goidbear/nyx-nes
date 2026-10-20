#pragma once
#include <cstdio>
#include <vector>
#include <array>
#include <string>
#include "types.h"

class Serializer {
public:
    virtual ~Serializer() = default;
    virtual void value(void* data, size_t size) = 0;
    virtual bool ok() const = 0;

    template<typename T> void field(T& v) { value(&v, sizeof(T)); }
    template<typename T, size_t N> void array(T (&a)[N]) { value(a, sizeof(T) * N); }
    template<typename T, size_t N> void array(std::array<T, N>& a) { value(a.data(), sizeof(T) * N); }
    template<typename T> void vec(std::vector<T>& v) {
        size_t n = v.size();
        value(&n, sizeof(n));
        v.resize(n);
        if (n > 0) value(v.data(), n * sizeof(T));
    }
};

class FileWriter : public Serializer {
    FILE* f = nullptr;
public:
    explicit FileWriter(const char* path) { f = fopen(path, "wb"); }
    ~FileWriter() { if (f) fclose(f); }
    void value(void* data, size_t size) override {
        if (f) fwrite(data, 1, size, f);
    }
    bool ok() const override { return f != nullptr; }
};

class FileReader : public Serializer {
    FILE* f = nullptr;
public:
    explicit FileReader(const char* path) { f = fopen(path, "rb"); }
    ~FileReader() { if (f) fclose(f); }
    void value(void* data, size_t size) override {
        if (f) fread(data, 1, size, f);
    }
    bool ok() const override { return f != nullptr; }
};
