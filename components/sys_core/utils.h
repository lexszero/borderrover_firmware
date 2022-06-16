#pragma once

#define __cat_internal(a, ...) a ## __VA_ARGS__
#define __cat(a, ...) __cat_internal(a, __VA_ARGS__)
#define __cat3(a, b, ...) __cat(a, __cat(b, __VA_ARGS__))

#define stringify(x) __stringify(x)
#define __stringify(x) #x

#include <cstring>
#include <memory>
#include <string>

typedef std::unique_ptr<char, void(*)(void*)> _auto_char_ptr;
static inline _auto_char_ptr auto_char_ptr(char *ptr) {
	return _auto_char_ptr(ptr, std::free);
}

static inline _auto_char_ptr auto_strdup(const char *ptr) {
	return auto_char_ptr(strdup(ptr));
}

static inline _auto_char_ptr auto_strndup(const char *ptr, size_t len) {
	return auto_char_ptr(strndup(ptr, len));
}
