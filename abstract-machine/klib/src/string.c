#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
   size_t size = 0;
   while (*s++) {
     size++;
   }
   return size;
  //panic("Not implemented");
}

//
char *strcpy(char *dst, const char *src) {
   char *ret = dst;
   while ((*dst++ = *src++))
     ;
   return ret;
  //panic("Not implemented");
}

char *strncpy(char *dst, const char *src, size_t n) {
   char *ret = dst;
   for (; *src && n; n--) {
     *dst++ = *src++;
   }
   while (n--) {
     *dst++ = 0;
   }
   return ret;
  //panic("Not implemented");
}


char *strcat(char *dst, const char *src) {
  panic("Not implemented");
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2 && *s1 == *s2) 
  {
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
  //panic("Not implemented");
}

int strncmp(const char *s1, const char *s2, size_t n) {
  panic("Not implemented");
}

void *memset(void *s, int c, size_t n) {
  char *dst = (char *)s;
  for (size_t i = 0; i < n; i++)
    dst[i] = c & 0xff;
  return s;
  //panic("Not implemented");
}

void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

void *memcpy(void *out, const void *in, size_t n) {
  uint8_t *_out = (uint8_t *)out;
  uint8_t *_in = (uint8_t *)in;
  while (n--)
    *_out++ = *_in++;
  return out;
  // panic("Not implemented");
}

int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
}

#endif
